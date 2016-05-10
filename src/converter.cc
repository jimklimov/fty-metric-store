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

/*
@header
    converter - Some helper functions to convert between types
@discuss
@end
*/

#include "agent_metric_store_classes.h"


bool 
    stobiosf (const std::string& string, int32_t& integer, int8_t& scale)
{
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
    int32_t integer_part = 0;
    int32_t fraction_part = 0;
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

int64_t
    string_to_int64 (const char *value)
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

//  --------------------------------------------------------------------------
//  Self test of this class

void
converter_test (bool verbose)
{
    printf (" * converter: ");

    //  @selftest

    int8_t scale = 0;
    int32_t integer = 0;
    assert ( stobiosf ("12.835", integer, scale));
    assert ( integer == 12835 );
    assert ( scale == -3 );
 
    assert ( stobiosf ("178746.2332", integer, scale));
    assert ( integer == 1787462332 );
    assert ( scale == -4 );
 
    assert ( stobiosf ("0.00004", integer, scale));
    assert ( integer == 4 );
    assert ( scale == -5 );

    assert ( stobiosf ("-12134.013", integer, scale) );
    assert ( integer == -12134013  );
    assert ( scale == -3 );

    assert ( stobiosf ("-1", integer, scale) );
    assert ( integer == -1  );
    assert ( scale == 0 );

    assert ( stobiosf ("-1.000", integer, scale) );
    assert ( integer == -1  );
    assert ( scale == 0 );

    assert ( stobiosf ("0", integer, scale) );
    assert ( integer == 0 );
    assert ( scale == 0 );
    
    assert ( stobiosf ("1", integer, scale) );
    assert ( integer == 1 );
    assert ( scale == 0 );

    assert ( stobiosf ("0.0", integer, scale) );
    assert ( integer == 0 );
    assert ( scale == 0 );

    assert ( stobiosf ("0.00", integer, scale) );
    assert ( integer == 0 );
    assert ( scale == 0 );

    assert ( stobiosf ("1.0", integer, scale) );
    assert ( integer == 1 );
    assert ( scale == 0 );

    assert ( stobiosf ("1.00", integer, scale) );
    assert ( integer == 1 );
    assert ( scale == 0 );

    assert ( stobiosf ("1234324532452345623541.00", integer, scale) == false );

    assert ( stobiosf ("2.532132356545624522452456", integer, scale) == false );
    
    assert ( stobiosf ("12x43", integer, scale) == false );
    assert ( stobiosf ("sdfsd", integer, scale) == false );
    
    assert ( string_to_int64( "1234" ) == 1234 );

    //  @end
    printf ("OK\n");
}
