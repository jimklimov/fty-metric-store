/*  =========================================================================
    fty_metric_store_classes - private header file

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
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
    =========================================================================
*/

#ifndef FTY_METRIC_STORE_CLASSES_H_INCLUDED
#define FTY_METRIC_STORE_CLASSES_H_INCLUDED

//  Platform definitions, must come first
#include "platform.h"

//  External API
#include "../include/fty_metric_store.h"

//  Extra headers

//  Opaque class structures to allow forward references
#ifndef LOGGER_T_DEFINED
typedef struct _logger_t logger_t;
#define LOGGER_T_DEFINED
#endif
#ifndef ACTOR_COMMANDS_T_DEFINED
typedef struct _actor_commands_t actor_commands_t;
#define ACTOR_COMMANDS_T_DEFINED
#endif
#ifndef CONVERTER_T_DEFINED
typedef struct _converter_t converter_t;
#define CONVERTER_T_DEFINED
#endif
#ifndef PERSISTANCE_T_DEFINED
typedef struct _persistance_t persistance_t;
#define PERSISTANCE_T_DEFINED
#endif
#ifndef MULTI_ROW_T_DEFINED
typedef struct _multi_row_t multi_row_t;
#define MULTI_ROW_T_DEFINED
#endif

//  Internal API

#include "logger.h"
#include "actor_commands.h"
#include "converter.h"
#include "persistance.h"
#include "multi_row.h"

//  *** To avoid double-definitions, only define if building without draft ***
#ifndef FTY_METRIC_STORE_BUILD_DRAFT_API

//  *** Draft method, defined for internal use only ***
//  Self test of this class.
FTY_METRIC_STORE_PRIVATE void
    logger_test (bool verbose);

//  *** Draft method, defined for internal use only ***
//  Self test of this class.
FTY_METRIC_STORE_PRIVATE void
    actor_commands_test (bool verbose);

//  *** Draft method, defined for internal use only ***
//  Self test of this class.
FTY_METRIC_STORE_PRIVATE void
    converter_test (bool verbose);

//  *** Draft method, defined for internal use only ***
//  Self test of this class.
FTY_METRIC_STORE_PRIVATE void
    persistance_test (bool verbose);

//  *** Draft method, defined for internal use only ***
//  Self test of this class.
FTY_METRIC_STORE_PRIVATE void
    multi_row_test (bool verbose);

//  Self test for private classes
FTY_METRIC_STORE_PRIVATE void
    fty_metric_store_private_selftest (bool verbose);

#endif // FTY_METRIC_STORE_BUILD_DRAFT_API

#endif
