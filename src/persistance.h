/*  =========================================================================
    persistance - Some helper functions for persistance layer

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

#include <functional>
#ifndef PERSISTANCE_H_INCLUDED
#define PERSISTANCE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


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


AGENT_METRIC_STORE_EXPORT
int
    insert_into_measurement(
        tntdb::Connection &conn,
        const char        *topic,
        m_msrmnt_value_t   value,
        m_msrmnt_scale_t   scale,
        int64_t            time,
        const char        *units,
        const char        *device_name);

AGENT_METRIC_STORE_EXPORT
int
    select_measurements (
        const std::string &connurl,
        const std::string &topic, // the whole topic XXX@YYY
        int64_t start_timestamp,
        int64_t end_timestamp,
        std::function<void(
                        const tntdb::Row&)>& cb,
        bool is_ordered);

AGENT_METRIC_STORE_EXPORT
int
    select_topic (
        const std::string &connurl,
        const std::string &topic, // the whole topic XXX@YYY
        std::function<void(
                        const tntdb::Row&)>& cb);

AGENT_METRIC_STORE_EXPORT
int
    delete_measurements(
        tntdb::Connection &conn,
        const char        *asset_name);

//  Self test of this class
AGENT_METRIC_STORE_EXPORT
void
    persistance_test (bool verbose);

AGENT_METRIC_STORE_EXPORT
void 
    flush_measurement_when_needed(std::string &url);

AGENT_METRIC_STORE_EXPORT
void
    flush_measurement(std::string &url);
//  @end

#ifdef __cplusplus
}
#endif

#endif
