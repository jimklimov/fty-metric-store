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
        body on success: a multipart message "OK"/B/C/D/E/F/G/[K_i/V_i]

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
                    "BAD_ELEMENT" when element in REQ does not exist
                    "BAD_QUANTITY" when quantity in REQ is not measured for the specified element
                    "BAD_STEP" when requested step is not supported
                    "BAD_TYPE" when type is not supported
                    "BAD_TIMERANGE" when in REQ fields  E and F do not form correct time interval

            example:
                "ERROR"/"BAD_MESSAGE"

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



//===============================================================
// XXX ACE: actually I think, that this knowledge doesn't belong
// to this agent, but it is necessary for REQ-REP
// to consider: move this somehow out of this agent
// for example: redefine protocol: to accept topic of the metric
#define AVG_STEPS_COUNT 7
const char *AVG_STEPS[AVG_STEPS_COUNT] = {
    "15m",
    "30m",
    "1h",
    "8h",
    "24h",
    "7d",
    "30d"
};

#define AVG_TYPES_COUNT 3
const char *AVG_TYPES[AVG_TYPES_COUNT] = {
    "arithmetic_mean",
    "min",
    "max"
};

bool is_average_step_supported (const char *step) {
    if (!step) {
        return false;
    }
    for (int i = 0; i < AVG_STEPS_COUNT; ++i) {
        if (strcmp (step, AVG_STEPS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool is_average_type_supported (const char *type) {
    if (!type) {
        return false;
    }
    for (int i = 0; i < AVG_TYPES_COUNT; ++i) {
        if (strcmp (type, AVG_TYPES[i]) == 0) {
            return true;
        }
    }
    return false;
}
//====================================================================




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
        // TODO fill it
        return msg_out;
    }

    zmsg_t *msg = *message_p;
    char *cmd = zmsg_popstr (msg);
    if ( !streq(cmd, "GET") ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }
    zstr_free(&cmd);

    char *element_name = zmsg_popstr (msg);
    if ( !element_name || streq (element_name, "") ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }

    char *quantity = zmsg_popstr (msg);
    if ( !quantity || streq (quantity, "") ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }

    char *step = zmsg_popstr (msg);
    if ( !step || is_average_step_supported (step) ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }

    char *aggr_type = zmsg_popstr (msg);
    if ( !aggr_type || is_average_type_supported (aggr_type) ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }

    char *start_time_str = zmsg_popstr (msg);
    if ( !start_time_str ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }

    char *end_time_str = zmsg_popstr (msg);
    if ( !end_time_str ) {
        zmsg_destroy (message_p);
        // TODO fill it
        return msg_out;
    }

    zstr_free (&end_time_str);
    zstr_free (&start_time_str);
    zstr_free (&aggr_type);
    zstr_free (&step);
    zstr_free (&quantity);
    zstr_free (&element_name);
    zmsg_destroy (message_p);
    return msg_out;
}

static void
s_handle_poll ()
{

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
    // TODO fill it
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

    uint64_t timestamp = (uint64_t) zclock_mono ();
    uint64_t timeout = (uint64_t) POLL_INTERVAL;

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, timeout);

        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted");
                break;
            }
            if (zpoller_expired (poller)) {
                s_handle_poll ();
            }
            timestamp = (uint64_t) zclock_mono ();
            continue;
        }

        uint64_t now = (uint64_t) zclock_mono ();
        if (now - timestamp >= timeout) {
            s_handle_poll ();
            timestamp = (uint64_t) zclock_mono ();
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
