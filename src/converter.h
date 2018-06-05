/*  =========================================================================
    converter - Some helper functions to convert between types
    Note: This file was manually amended, see below

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

#ifndef CONVERTER_H_INCLUDED
#define CONVERTER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


/**
 *  \brief Take string encoded double value and if possible
 *          return representation: integer x 10^scale
 */
FTY_METRIC_STORE_EXPORT bool
    stobiosf (const std::string& string, int32_t& integer, int8_t& scale);

FTY_METRIC_STORE_EXPORT int64_t
    string_to_int64 (const char *value);

FTY_METRIC_STORE_EXPORT bool
    stobiosf_wrapper (const std::string& string, int32_t& integer, int8_t& scale);

//  Self test of this class
//  Note: Keep this definition in sync with fty_metric_store_classes.h
FTY_METRIC_STORE_PRIVATE void
    converter_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
