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
    bios_agent_ms_server - Actor listening on metrics with request reply protocol for graphs
@discuss
@end
*/

#include "agent_metric_store_classes.h"

void
bios_agent_ms_server (zsock_t *pipe, void* args)
{
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
