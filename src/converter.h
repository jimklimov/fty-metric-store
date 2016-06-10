/*  =========================================================================
    converter - Some helper forntions to convert between types

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

#ifndef CONVERTER_H_INCLUDED
#define CONVERTER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


/**
 *  \brief Take string encoded double value and if possible 
 *          return representation: integer x 10^scale
 */
AGENT_METRIC_STORE_EXPORT bool
    stobiosf (const std::string& string, int32_t& integer, int8_t& scale);

AGENT_METRIC_STORE_EXPORT int64_t
    string_to_int64 (const char *value);

AGENT_METRIC_STORE_EXPORT bool
    stobiosf_wrapper (const std::string& string, int32_t& integer, int8_t& scale);

//  Self test of this class
AGENT_METRIC_STORE_EXPORT
void
    converter_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
