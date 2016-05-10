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
@end
*/

#include "agent_metric_store_classes.h"

#define POLL_INTERVAL 10000

// ----- table:  t_bios_measurement -------------------
// ----- column: value --------------------------------
typedef  int32_t m_msrmnt_value_t;

// ----- table:  t_bios_measurement -------------------
// ----- column: scale --------------------------------
typedef  int16_t  m_msrmnt_scale_t;

// ----- table:  t_bios_measurement_topic -------------
// ----- column: id  ----------------------------------
typedef uint16_t  m_msrmnt_tpc_id_t;

// ----- table:  t_bios_discovered_device -------------
// ----- column: id_discovered_device -----------------
typedef uint16_t m_dvc_id_t;
/*!
 \brief Take string encoded double value and if possible return representation: integer x 10^scale
*/
bool
    stobiosf (const std::string& string, int32_t& integer, int8_t& scale);

bool 
stobiosf (const std::string& string, int32_t& integer, int8_t& scale) {
    // Note: Shall performance __really__ become an issue, consider
    // http://stackoverflow.com/questions/1205506/calculating-a-round-order-of-magnitude
    if (string.empty ())
        return false;

    // See if string is encoded double
    size_t pos = 0;
    double temp = 0;
    try {
        temp = std::stod (string, &pos);
    }
    catch (...) {
        return false;
    }
    if (pos != string.size () || std::isnan (temp) || std::isinf (temp)) {
        return false;
    }

    // parse out the string
    std::string integer_string, fraction_string;
    int32_t integer_part = 0, fraction_part = 0;
    std::string::size_type comma = string.find (".");
    bool minus = false;

    integer_string = string.substr (0, comma);
    try {
        integer_part = std::stoi (integer_string);
    }
    catch (...) {
        return false;
    }
    if (integer_part < 0)
        minus = true;
    if (comma ==  std::string::npos) {
        scale = 0;
        integer = integer_part;
        return true;
    }
    fraction_string = string.substr (comma+1);
    // strip zeroes from right
    while (!fraction_string.empty ()  && fraction_string.back () == 48) {
        fraction_string.resize (fraction_string.size () - 1);
    }
    if (fraction_string.empty ()) {
        scale = 0;
        integer = integer_part;
        return true;
    }
    std::string::size_type fraction_size = fraction_string.size ();
    try {
        fraction_part = std::stoi (fraction_string);
    }
    catch (...) {
        return false;
    }

    int64_t sum = integer_part;
    for (std::string::size_type i = 0; i < fraction_size; i++) {
        sum = sum * 10;
    }
    if (minus)
        sum = sum - fraction_part;
    else
        sum = sum + fraction_part;
    
    if ( sum > std::numeric_limits<int32_t>::max ()) {
        return false;
    }
    if (fraction_size - 1 > std::numeric_limits<int8_t>::max ()) {
        return false;
    }
    scale = -fraction_size;
    integer = static_cast <int32_t> (sum);
    return true;
}



static int64_t
    string_to_int64( const char *value )
{
    char *end;
    int64_t result;
    errno = 0;
    if( ! value ) {
        errno = EINVAL;
        return INT64_MAX;
    }
    result = strtoll( value, &end, 10 );
    if( *end ) {
        errno = EINVAL;
    }
    if( errno ) {
        return INT64_MAX;
    }
    return result;
}


m_dvc_id_t
    insert_as_not_classified_device(
        tntdb::Connection &conn,
        const char        *device_name)
{
    if(device_name == NULL || device_name[0] == 0 ){
        log_error("[t_bios_discovered_device] can't insert a device with NULL or non device name");
        return 0;
    }
    m_dvc_id_t id_discovered_device=0;
    tntdb::Statement st;
    st = conn.prepareCached(
        " INSERT INTO"
        "   t_bios_discovered_device"
        "     (name, id_device_type)"
        " SELECT"
        "   :name,"
        "   (SELECT T.id_device_type FROM t_bios_device_type T WHERE T.name = 'not_classified')"
        " FROM"
        "   ( SELECT NULL name, 0 id_device_type ) tbl"
        " WHERE :name NOT IN (SELECT name FROM t_bios_discovered_device )"
     );
    uint32_t n = st.set("name", device_name).execute();
    id_discovered_device=conn.lastInsertId();
    log_debug("[t_discovered_device]: device '%s' inserted %" PRIu32 " rows ",
              device_name, n);
    if( n == 1 ) {
        // try to update  relation table
        st = conn.prepareCached(
            " INSERT INTO"
            "   t_bios_monitor_asset_relation (id_discovered_device, id_asset_element)"
            " SELECT"
            "   DD.id_discovered_device, AE.id_asset_element"
            " FROM"
            "   t_bios_discovered_device DD INNER JOIN t_bios_asset_element AE on DD.name = AE.name"
            " WHERE"
            "   DD.name = :name AND"
            "   DD.id_discovered_device NOT IN ( SELECT id_discovered_device FROM t_bios_monitor_asset_relation )"
        );
        n = st.set("name", device_name).execute();
        log_debug("[t_bios_monitor_asset_relation]: inserted %" PRIu32 " rows about %s", n, device_name);
    }else{
        log_error("[t_discovered_device]:  device %s not inserted", device_name );
    }
    return id_discovered_device;
}


//return id_discovered_device or 0 in case of issue
m_dvc_id_t
    prepare_discovered_device(
        tntdb::Connection &conn,
        const char        *device_name)
{
    assert (device_name);

    // verify if the device name exists in t_bios_discovered_device
    // if not create it as not_classified device type
    m_dvc_id_t id_discovered_device = 0;
    tntdb::Statement st = conn.prepareCached(
            " SELECT id_discovered_device "
            " FROM "
            "    t_bios_discovered_device v"
            " WHERE "
            "   v.name = :name");
    st.set("name", device_name);
    try{
        tntdb::Row row=st.selectRow();
        row["id_discovered_device"].get(id_discovered_device);
        return id_discovered_device;
    }catch(const tntdb::NotFound &e){
        log_debug("[t_bios_discovered_device] device %s not found => try to create it as not classified",device_name);
        //add the device as 'not_classified' type
        // probably device doesn't exist in t_bios_discovered_device. Let's fill it.
        return insert_as_not_classified_device(conn,device_name);
    }
}


//return topic_id or 0 in case of issue
m_msrmnt_tpc_id_t
    prepare_topic(
        tntdb::Connection &conn,
        const char        *topic,
        const char        *units,
        const char        *device_name)
{
    assert ( topic );
    assert ( units );
    assert ( device_name );

    m_dvc_id_t id_discovered_device = prepare_discovered_device (conn, device_name);
    if ( id_discovered_device == 0 ) {
        return 0;
    }
    try {
        tntdb::Statement st =  conn.prepareCached(
            " INSERT INTO "
            "   t_bios_measurement_topic "
            "    (topic, units, device_id) "
            " VALUES (:topic, :units, :device_id) "
            " ON DUPLICATE KEY "
            "   UPDATE "
            "      id = LAST_INSERT_ID(id) "
        );

        uint32_t n = st.set("topic", topic)
                       .set("units", units)
                       .set("device_id", id_discovered_device)
                       .execute();

        m_msrmnt_tpc_id_t topic_id = conn.lastInsertId();
        if ( topic_id != 0 ) {
            log_debug("[t_bios_measurement_topic]: inserted topic %s, #%" PRIu32 " rows , topic_id %u", topic, n, topic_id);
        } else {
            log_error("[t_bios_measurement_topic]:  topic %s not inserted", topic);
        }
        return topic_id;
    } catch(const std::exception &e) {
        log_error ("Topic '%s' was not inserted with error: %s", topic, e.what());
        return 0;
    }
}


int
    insert_into_measurement(
        tntdb::Connection &conn,
        const char        *topic,
        m_msrmnt_value_t   value,
        m_msrmnt_scale_t   scale,
        int64_t            time,
        const char        *units,
        const char        *device_name)
{
    assert ( units );
    assert ( device_name );
    assert ( topic );

    if ( topic[0]=='@' ) {
        log_error ("malformed value of topic '%s' is not allowed", topic);
        return 1;
    }

    try {
        m_msrmnt_tpc_id_t topic_id = prepare_topic(conn, topic, units, device_name);
        if ( topic_id == 0 ) {
            // topic was not inserted -> cannot insert metric
            return 1;
        }
        tntdb::Statement st = conn.prepareCached (
                "INSERT INTO t_bios_measurement (timestamp, value, scale, topic_id) "
                "  VALUES (:time, :value, :scale, :topic_id)"
                "  ON DUPLICATE KEY UPDATE value = :value, scale = :scale");
        st.set("time",  time)
            .set("value", value)
            .set("scale", scale)
            .set("topic_id", topic_id)
            .execute();

        log_debug("[t_bios_measurement]: inserted " \
                "value:%" PRIi32 " * 10^%" PRIi16 " %s "\
                "topic = '%s' topic_id=%" PRIi16 " time = %" PRIu64,
                value, scale, units, topic, topic_id, time);
        return 0;
    } catch(const std::exception &e) {
        log_error ("Metric with topic '%s' was not inserted with error: %s", topic, e.what());
        return 1;
    }
}


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

static void
s_handle_mailbox (mlm_client_t *client, zmsg_t **message_p)
{
   assert (client);
   assert (message_p && *message_p);

   log_error ("Mailbox command is not implemented.");

   zmsg_destroy (message_p);
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

    // TODO adopt tests from core
    /*
TEST_CASE ("stobiosf", "[utilities]") {
    int8_t scale = 0;
    int32_t integer = 0;
    CHECK (utils::math::stobiosf ("12.835", integer, scale));
    CHECK ( integer == 12835 );
    CHECK ( scale == -3 );
 
    CHECK (utils::math::stobiosf ("178746.2332", integer, scale));
    CHECK ( integer == 1787462332 );
    CHECK ( scale == -4 );
 
    CHECK (utils::math::stobiosf ("0.00004", integer, scale));
    CHECK ( integer == 4 );
    CHECK ( scale == -5 );

    CHECK ( utils::math::stobiosf ("-12134.013", integer, scale) );
    CHECK ( integer == -12134013  );
    CHECK ( scale == -3 );

    CHECK ( utils::math::stobiosf ("-1", integer, scale) );
    CHECK ( integer == -1  );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("-1.000", integer, scale) );
    CHECK ( integer == -1  );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("0", integer, scale) );
    CHECK ( integer == 0 );
    CHECK ( scale == 0 );
    
    CHECK ( utils::math::stobiosf ("1", integer, scale) );
    CHECK ( integer == 1 );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("0.0", integer, scale) );
    CHECK ( integer == 0 );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("0.00", integer, scale) );
    CHECK ( integer == 0 );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("1.0", integer, scale) );
    CHECK ( integer == 1 );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("1.00", integer, scale) );
    CHECK ( integer == 1 );
    CHECK ( scale == 0 );

    CHECK ( utils::math::stobiosf ("1234324532452345623541.00", integer, scale) == false );

    CHECK ( utils::math::stobiosf ("2.532132356545624522452456", integer, scale) == false );
    
    CHECK ( utils::math::stobiosf ("12x43", integer, scale) == false );
    CHECK ( utils::math::stobiosf ("sdfsd", integer, scale) == false );
}*/
    //  @end
    printf ("OK\n");
}
