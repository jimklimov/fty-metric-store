/*  =========================================================================
    fty_metric_store_server - Metric store actor

    Copyright (C) 2014 - 2017 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_metric_store_server - Actor listening on metrics with request reply protocol for graphs
@discuss


== Protocol for aggregated data - see README for the description.
    Example request:
                "8CB3E9A9649B"/"GET/"asset_test"/"realpower.default"/"24h"/"min"/"1234567"/"1234567890"/"0"
    Example reply on success:
                "8CB3E9A9649B"/"OK"/"asset_test"/"realpower.default"/"24h"/"min"/"1234567"/"1234567890"/"0"/"W"/"1234567"/"88.0"/"123456556"/"99.8"
    Example reply on error:
                "8CB3E9A9649B"/"ERROR"/"BAD_MESSAGE"

    Supported reasons for errors are:
            "BAD_MESSAGE" when REQ does not conform to the expected message structure (but still includes <uuid>)
            "BAD_TIMERANGE" when in REQ fields 'start' and 'end' do not form correct time interval
            "INTERNAL_ERROR" when error occured during fetching the rows
            "BAD_REQUEST" requested information is not monitored by the system
                    (missing record in the t_bios_measurement_table)
            "BAD_ORDERED" when parameter 'ordering_flag' does not have allowed value

    If the request message does not include <uuid>, behaviour is undefined.
    If the subject is incorrect, fty-metric-store server responds with ERROR/UNSUPPORTED_SUBJECT.

@end
*/

#include "fty_metric_store_classes.h"

#define POLL_INTERVAL 1000

#define AVG_GRAPH "aggregated data"
/**
 *  \brief A connection string to the database
 *
 *  TODO: if DB_USER or DB_PASSWD would be changed the daemon
 *          should be restarted in order to apply changes
 */
static std::string url =
    std::string("mysql:db=box_utf8;user=") +
    ((getenv("DB_USER")   == NULL) ? "root" : getenv("DB_USER")) +
    ((getenv("DB_PASSWD") == NULL) ? ""     :
    std::string(";password=") + getenv("DB_PASSWD"));

// destroys the message
static zmsg_t*
s_handle_aggregate (mlm_client_t *client, zmsg_t **message_p)
{
    assert (client);
    assert (message_p && *message_p);

    zmsg_t *msg = *message_p;
    zmsg_t *msg_out = zmsg_new ();

    if ( zmsg_size(msg) < 8 ) {
        zmsg_destroy (message_p);
        log_error ("Message has unsupported format, ignore it");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        return msg_out;
    }

    char *cmd = zmsg_popstr (msg);
    if ( !streq(cmd, "GET") ) {
        zmsg_destroy (message_p);
        log_error ("GET is misssing");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        return msg_out;
    }
    zstr_free(&cmd);
    // All declarations are before first "goto"
    // to make compiler happy
    char *asset_name = zmsg_popstr (msg);
    char *quantity = zmsg_popstr (msg);
    char *step = zmsg_popstr (msg);
    char *aggr_type = zmsg_popstr (msg);
    char *start_date_str = zmsg_popstr (msg);
    char *end_date_str = zmsg_popstr (msg);
    char *ordered = zmsg_popstr (msg);
    bool is_ordered = false;
    int64_t start_date = 0;
    int64_t end_date = 0;
    std::string topic;
    std::function <void(const tntdb::Row &)> add_measurement;
    std::function <void(const tntdb::Row &)> select_units;
    std::string units;
    int rv;

    if ( !asset_name || streq (asset_name, "") ) {
        log_error ("asset name is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !quantity || streq (quantity, "") ) {
        log_error ("quantity is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !step ) {
        log_error ("step is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !aggr_type ){
        log_error ("type of the aggregaation is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !start_date_str ) {
        log_error ("start date is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !end_date_str ) {
        log_error ("end date is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !ordered ) {
        log_error ("ordered is empty");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    start_date = string_to_int64 (start_date_str);
    if (errno != 0) {
        errno = 0;
        log_error ("start date cannot be converted to number");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    end_date = string_to_int64 (end_date_str);
    if (errno != 0) {
        errno = 0;
        log_error ("end date cannot be converted to number");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( start_date > end_date ) {
        log_error ("start date > end date");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_TIMERANGE");
        goto exit;
    }

    if ( !streq (ordered, "1") && !streq (ordered, "0") ) {
        log_error ("ordered is not 1/0");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_ORDERED");
        goto exit;
    }

    topic += quantity;
    topic += "_"; // TODO: when ecpp files would be changed -> take another character
    topic += aggr_type;
    topic += "_"; // TODO: when ecpp files would be changed -> take another character
    topic += step;
    topic += "@";
    topic += asset_name;

    select_units = [&units](const tntdb::Row& r)
        {
            r["units"].get(units);
        };

    rv = select_topic (url, topic, select_units);
    if ( rv ) {
        // as we have prepared it for SUCCESS, but we failed in the end
        zmsg_addstr (msg_out, "ERROR");
        if ( rv == -2 ) {
            log_error ("average request: topic is not found");
            zmsg_addstr (msg_out, "BAD_REQUEST");
        } else {
            log_error ("average request: unexpected error during topic selecting");
            zmsg_addstr (msg_out, "INTERNAL_ERROR");
        }
        goto exit;
    }

    zmsg_addstr (msg_out, "OK");
    zmsg_addstr (msg_out, asset_name);
    zmsg_addstr (msg_out, quantity);
    zmsg_addstr (msg_out, step);
    zmsg_addstr (msg_out, aggr_type);
    zmsg_addstr (msg_out, start_date_str);
    zmsg_addstr (msg_out, end_date_str);
    zmsg_addstr (msg_out, ordered);
    zmsg_addstr (msg_out, units.c_str());

    add_measurement = [&msg_out](const tntdb::Row& r)
        {
            m_msrmnt_value_t value = 0;
            r["value"].get(value);

            m_msrmnt_scale_t scale = 0;
            r["scale"].get(scale);
            double real_value = value * std::pow (10, scale);

            int64_t timestamp = 0;
            r["timestamp"].get(timestamp);

            zmsg_addstr (msg_out, std::to_string(timestamp).c_str());
            zmsg_addstr (msg_out, std::to_string(real_value).c_str());
        };

    is_ordered = streq (ordered, "1");
    rv = select_measurements (url, topic, start_date, end_date, add_measurement, is_ordered);
    if ( rv ) {
        // as we have prepared it for SUCCESS, but we failed in the end
        log_error ("unexpected error during measurement selecting");
        zmsg_destroy (&msg_out);
        msg_out = zmsg_new ();
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "INTERNAL_ERROR");
    }

exit:
    zstr_free (&ordered);
    zstr_free (&end_date_str);
    zstr_free (&start_date_str);
    zstr_free (&aggr_type);
    zstr_free (&step);
    zstr_free (&quantity);
    zstr_free (&asset_name);
    zmsg_destroy (message_p);
    return msg_out;
}

static void
s_handle_service (mlm_client_t *client, zmsg_t **message_p)
{
    assert (client);
    assert (message_p && *message_p);

    log_error ("Service deliver is not implemented.");

    zmsg_destroy (message_p);
}

static void
s_handle_mailbox (mlm_client_t *client, zmsg_t **message_p)
{
    assert (client);
    assert (message_p && *message_p);

    zmsg_t *msg = *message_p;

    if (zmsg_size (msg) == 0) {
        log_info ("Empty message with subject %s from %s, ignoring", mlm_client_subject (client), mlm_client_sender (client));
        zmsg_destroy (message_p);
    }

    zmsg_t *msg_out;
    char *uuid = zmsg_popstr (msg);
    if ( streq ( mlm_client_subject (client), AVG_GRAPH ) ) {
        msg_out = s_handle_aggregate (client, message_p);
    } else {
        log_info ("Bad subject %s from %s, ignoring", mlm_client_subject (client), mlm_client_sender (client));
        zmsg_destroy (message_p);
        msg_out = zmsg_new ();
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "UNSUPPORTED_SUBJECT");
    }
    zmsg_pushstr (msg_out, uuid);
    zstr_free (&uuid);
    mlm_client_sendto (client, mlm_client_sender(client), mlm_client_subject(client), NULL, 1000, &msg_out);
}

static void
s_process_metric (fty_proto_t *m)
{
    assert(m);
    // TODO: implement FTY_STORE_AGE_ support
    // ignore the stuff not coming from computation module
    if (!fty_proto_aux_string (m, "x-cm-type", NULL)) {
        return;
    }
    std::string db_topic = std::string (fty_proto_type (m)) + "@" + std::string(fty_proto_name (m));

    m_msrmnt_value_t value = 0;
    m_msrmnt_scale_t scale = 0;
    if (!strstr (fty_proto_value (m), ".")) {
        value = string_to_int64 (fty_proto_value (m));
        if (errno != 0) {
            errno = 0;
            log_error ("value '%s' of the metric is not integer", fty_proto_value (m) );
            return;
        }
    }
    else {
        int8_t lscale = 0;
        int32_t integer = 0;
        if (!stobiosf_wrapper (fty_proto_value (m), integer, lscale)) {
            log_error ("value '%s' of the metric is not double", fty_proto_value (m));
            return;
        }
        value = integer;
        scale = lscale;
    }

    // time is a time when message was received
    uint64_t _time = fty_proto_time (m);
    tntdb::Connection conn;
    try {
        conn = tntdb::connectCached(url);
        conn.ping();
    } catch (const std::exception &e) {
        log_error("Can't connect to the database");
        return;
    }

    insert_into_measurement(
            conn, db_topic.c_str(), value, scale, _time,
            fty_proto_unit (m), fty_proto_name (m));
}

static void
s_process_asset (fty_proto_t *m)
{
    assert(m);
    if ( streq (fty_proto_operation (m), "delete") ) {
        log_debug ("Asset is deleted -> delete all it measurements");
        tntdb::Connection conn;
        try {
            conn = tntdb::connectCached(url);
            conn.ping();
        } catch (const std::exception &e) {
            log_error("Can't connect to the database");
            return;
        }

        delete_measurements (conn, fty_proto_name(m));
    } else {
        log_debug ("Operation '%s' on the asset is not interesting", fty_proto_operation (m) );
    }
}

static void
s_handle_stream (mlm_client_t *client, zmsg_t **message_p)
{
    assert (client);
    assert (message_p && *message_p);

    fty_proto_t *m = fty_proto_decode (message_p);
    if ( !m ) {
        log_error("Can't decode the fty_proto message, ignore it");
        return;
    }

    if ( fty_proto_id(m) == FTY_PROTO_METRIC ) {
        s_process_metric (m);
    } else if ( fty_proto_id(m) == FTY_PROTO_ASSET ) {
        s_process_asset (m);
    } else {
        log_error ("Unsupported fty_proto message with id = '%d'", fty_proto_id(m));
    }
    fty_proto_destroy (&m);
    zmsg_destroy (message_p);
}

void
fty_metric_store_server (zsock_t *pipe, void* args)
{
    mlm_client_t *client = mlm_client_new ();
    if (!client) {
        log_error ("mlm_client_new () failed");
        return;
    }

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    if (!poller) {
        log_error ("zpoller_new () failed");
        mlm_client_destroy (&client);
        return;
    }

    zsock_signal (pipe, 0);

    uint64_t timeout = (uint64_t) POLL_INTERVAL;

    uint64_t last = zclock_mono ();
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, timeout);
        uint64_t now = zclock_mono();
        if (now - last >= timeout) {
            last = now;
            //do a periodic flush
            flush_measurement_when_needed(url);
        }
        if (which == NULL) {
            if (zpoller_expired (poller) && !zsys_interrupted ){
                continue;
            }

            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted");
                break;
            }
            continue;
        }

        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            if (!message) {
                log_error ("Given `which == pipe`, function `zmsg_recv (pipe)` returned NULL");
                continue;
            }
            if (actor_commands (client, &message) == 1) {
                break;
            }
            continue;
        }

        // paranoid non-destructive assertion of a twisted mind
        if (which != mlm_client_msgpipe (client)) {
            log_error ("which was checked for NULL, pipe and now should have been `mlm_client_msgpipe (client)` but is not.");
            continue;
        }

        zmsg_t *message = mlm_client_recv (client);
        if (!message) {
            log_error ("Given `which == mlm_client_msgpipe (client)`, function `mlm_client_recv ()` returned NULL");
            continue;
        }

        const char *command = mlm_client_command (client);
        if (streq (command, "STREAM DELIVER")) {
            s_handle_stream (client, &message);
        }
        else
        if (streq (command, "MAILBOX DELIVER")) {
            s_handle_mailbox (client, &message);
        }
        else
        if (streq (command, "SERVICE DELIVER")) {
            s_handle_service (client, &message);
        }
        else {
            log_error ("Unrecognized mlm_client_command () = '%s'", command ? command : "(null)");
        }

        zmsg_destroy (&message);
    } // while (!zsys_interrupted)
    flush_measurement(url);

    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_metric_store_server_test (bool verbose)
{
    static const char *endpoint = "inproc://malamute-test";

    printf (" * fty_metric_store_server: ");
    ManageFtyLog::setInstanceFtylog("fty_metric_store_server");

    zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (server, "BIND", endpoint, NULL);
    if (verbose) {
        zstr_send (server, "VERBOSE");
        ManageFtyLog::getInstanceFtylog()->setVeboseMode();
    }

    zactor_t *self = zactor_new (fty_metric_store_server, (void*) NULL);
    zstr_sendx (self, "CONNECT", endpoint, "fty-metric-store", NULL);

    log_trace ("Test for mailbox request error handling");
    mlm_client_t *mbox_client = mlm_client_new();
    assert(mlm_client_connect(mbox_client, endpoint, 5000, "mbox-query") >= 0);

    static const char *uuid = "012345679";
    zmsg_t *msg = zmsg_new();
    zmsg_addstr (msg, uuid);
    zmsg_addstr (msg, "GET");
    zmsg_addstr (msg, "some-asset");
    zmsg_addstr (msg, "realpower.default");
    zmsg_addstr (msg, "15m");
    zmsg_addstr (msg, "min");
    zmsg_addstr (msg, "0");
    zmsg_addstr (msg, "9999");
    zmsg_addstr (msg, "1");
    //  This will fail because either the database is not accessible or the
    //  asset does not exist
    assert (mlm_client_sendto (mbox_client, "fty-metric-store", AVG_GRAPH, NULL, 1000, &msg) >= 0);
    assert ((msg = mlm_client_recv (mbox_client)));
    char *received_uuid = zmsg_popstr (msg);
    assert (streq (uuid, received_uuid));
    zstr_free (&received_uuid);
    zmsg_print (msg);
    zmsg_destroy (&msg);

    mlm_client_destroy(&mbox_client);
    zactor_destroy(&self);
    zactor_destroy(&server);

    printf ("OK.\n");
}
