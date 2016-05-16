/*  =========================================================================
    bios_agent_ms_server - Actor listening on metrics with request reply protocol for graphs

    Copyright (C) 2014 - 2015 Eaton                                        
                                                                           
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
    bios_agent_ms_server - Actor listening on metrics with request
                reply protocol for graphs
@discuss

REQ-REP:
    request:
        subject: "aggregated data"
        body: a multipart string message "GET"/A/B/C/D/E/F
   
            where:
                A - element name
                B - quantity
                C - step (15m, 24h, 7d, 30d)
                D - type (min, max, arithmetic_mean)
                E - start timestamp (UTC unix timestamp)
                F - end timestamp (UTC unix timestamp)

            example:
                "GET/"asset_test"/"realpower.default"/"24h"/"min"/"1234567"/"1234567890"

    reply:
        subject: "aggregated data"
        body on success: a multipart string message "OK"/A/B/C/D/E/F/G/[K_i/V_i]

            where:
                A - F have the same meaning as in "request" and must be repeated
                G - units
                K_i - key (UTC unix timestamp)
                V_i - value (value)

            example:
                "OK"/"asset_test"/"realpower.default"/"24h"/"min"/"1234567"/"1234567890"/"W"/"1234567"/"88.0"/"123456556"/"99.8"


        body on error: a multipart string message "ERROR"/R

            where:
                R - string describing the reason of the error
                    "BAD_MESSAGE" when REQ does not conform to the expected message structure
                    "BAD_TIMERANGE" when in REQ fields  E and F do not form correct time interval
                    "INTERNAL_ERROR" when error occured during fetching the rows

            example:
                "ERROR"/"BAD_MESSAGE"

    request:
        subject: UNSUPPORTED_SUBJECT
        body: any message
    
    reply:
        subject: UNSUPPORTED_SUBJECT
        body: a multipart string message "ERROR"/"UNSUPPORTED_SUBJECT"

@end
*/

#include "agent_metric_store_classes.h"

#define POLL_INTERVAL 10000

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
    zmsg_t *msg_out = zmsg_new(); 
    if ( zmsg_size(*message_p) < 7 ) {
        zmsg_destroy (message_p);
        log_error ("Message has unsupported format, ignore it");
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        return msg_out;
    }

    zmsg_t *msg = *message_p;
    char *cmd = zmsg_popstr (msg);
    if ( !streq(cmd, "GET") ) {
        zmsg_destroy (message_p);
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

    int64_t start_date = 0;
    int64_t end_date = 0;
    std::string topic;
    std::string units;
    std::function <void(const tntdb::Row &)> add_measurement;
    int rv;

    if ( !asset_name || streq (asset_name, "") ) {
        goto exit;
    }

    if ( !quantity || streq (quantity, "") ) {
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !step ) {
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !aggr_type ){
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !start_date_str ) {
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( !end_date_str ) {
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    start_date = string_to_int64 (start_date_str);
    if (errno != 0) {
        errno = 0;
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    end_date = string_to_int64 (end_date_str);
    if (errno != 0) {
        errno = 0;
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_MESSAGE");
        goto exit;
    }

    if ( start_date > end_date ) {
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "BAD_TIMERANGE");
        goto exit;
    }
    
    topic += quantity;
    topic += "_"; // TODO: when ecpp files would be changed -> take another character
    topic += aggr_type;
    topic += "_"; // TODO: when ecpp files would be changed -> take another character
    topic += step;
    topic += "@";
    topic += asset_name;


    zmsg_addstr (msg_out, "OK");
    zmsg_addstr (msg_out, asset_name);
    zmsg_addstr (msg_out, quantity);
    zmsg_addstr (msg_out, step);
    zmsg_addstr (msg_out, aggr_type);
    zmsg_addstr (msg_out, start_date_str);
    zmsg_addstr (msg_out, end_date_str);
    
    add_measurement = [&msg_out, &units](const tntdb::Row& r)
        {
            if ( units.empty() ) {
                // this code should be called only one and only for the first fetched row
                r["units"].get(units);
                zmsg_addstr (msg_out, units.c_str());
            }

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

    rv = select_measurements (url, topic, start_date, end_date, add_measurement);
    if ( !rv ) {
        // as we have prepared it for SUCCESS, but we failed in the end
        zmsg_destroy (&msg_out);
        msg_out = zmsg_new ();
        zmsg_addstr (msg_out, "ERROR");
        zmsg_addstr (msg_out, "INTERNAL_ERROR");
    }

exit:
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

static zmsg_t*
s_handle_mailbox (mlm_client_t *client, zmsg_t **message_p)
{
    assert (client);
    assert (message_p && *message_p);
    if ( streq ( mlm_client_subject (client), AVG_GRAPH ) ) {
        return s_handle_aggregate (client, message_p);
    }
    log_error ("Unsupported subject '%s'",  mlm_client_subject (client));
    zmsg_destroy (message_p);
    zmsg_t *msg_out = zmsg_new(); 
    zmsg_addstr (msg_out, "ERROR");
    zmsg_addstr (msg_out, "UNSUPPORTED_SUBJECT");
    return msg_out;
}

static void
s_handle_stream (mlm_client_t *client, zmsg_t **message_p)
{
    assert (client);
    assert (message_p && *message_p);

    bios_proto_t *m = bios_proto_decode (message_p);
    if ( !m ) {
        log_error("Can't decode the biosproto message, ignore it");
        return;
    }
    // TODO check if it is metric
    // TODO check if it is computed
    std::string db_topic = std::string (bios_proto_type (m)) + "@" + std::string(bios_proto_element_src (m));

    m_msrmnt_value_t value = 0;
    m_msrmnt_scale_t scale = 0;
    if (!strstr (bios_proto_value (m), ".")) {
        value = string_to_int64 (bios_proto_value (m));
        if (errno != 0) {
            errno = 0;
            log_error ("value of the metric is not integer");
            bios_proto_destroy (&m);
            zmsg_destroy (message_p);
            return;
        }
    }
    else {
        int8_t lscale = 0;
        int32_t integer = 0;
        if (!stobiosf (bios_proto_value (m), integer, lscale)) {
            log_error ("value of the metric is not double");
            bios_proto_destroy (&m);
            zmsg_destroy (message_p);
            return;
        }
        value = integer;
        scale = lscale;
    }

    // time is a time when message was received
    uint64_t _time = bios_proto_aux_number(m, "time", ::time(NULL));
    tntdb::Connection conn;
    try {
        conn = tntdb::connectCached(url);
        conn.ping();
    } catch (const std::exception &e) {
        log_error("Can't connect to the database");
        zmsg_destroy (message_p);
        return;
    }

    insert_into_measurement(
            conn, db_topic.c_str(), value, scale, _time,
            bios_proto_unit (m), bios_proto_element_src (m));
    bios_proto_destroy (&m);
    zmsg_destroy (message_p);
}

void
bios_agent_ms_server (zsock_t *pipe, void* args)
{
    mlm_client_t *client = mlm_client_new ();
    if (!client) {
        log_critical ("mlm_client_new () failed");
        return;
    }

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    if (!poller) {
        log_critical ("zpoller_new () failed");
        mlm_client_destroy (&client);
        return;
    }

    zsock_signal (pipe, 0);

    uint64_t timeout = (uint64_t) POLL_INTERVAL;

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, timeout);

        if (which == NULL) {
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
            log_critical ("which was checked for NULL, pipe and now should have been `mlm_client_msgpipe (client)` but is not.");
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

    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
bios_agent_ms_server_test (bool verbose)
{
    printf (" * bios_agent_ms_server: ");
    //  @selftest
    //  @end
    printf ("OK\n");
}
