/*  =========================================================================
    fty_metric_store - Metric store agent

    Copyright (C) 2014 - 2020 Eaton

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
    fty_metric_store - Metric store agent
@discuss
@end
*/

#include "fty_metric_store_classes.h"

#include <getopt.h>

static const char *AGENT_NAME = "fty-metric-store";
static const char *ENDPOINT = "ipc://@/malamute";

#define STEPS_SIZE 8
static const char *STEPS[STEPS_SIZE]    = {"RT", "15m", "30m", "1h", "8h", "1d", "7d", "30d"};
static const char *DEFAULTS[STEPS_SIZE] = { "0",   "1",   "1",  "7",  "7", "30", "30", "180"};

void usage () {
    puts (
        "fty-metric-store [options] ...\n"
        "  --verbose / -v         verbose mode\n"
        "  --config-file / -c     TODO\n"
        "  --help / -h            this information\n"
    );
}

int main (int argc, char *argv [])
{
    ManageFtyLog::setInstanceFtylog(AGENT_NAME, FTY_COMMON_LOGGING_DEFAULT_CFG);

// Some systems define struct option with non-"const" "char *"
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif
    static const char *short_options = "hvc:";
    static struct option long_options[] =
    {
        {"help",            no_argument,        0,  1},
        {"verbose",         no_argument,        0,  'v'},
        {"config-file",     required_argument,  0,  'c'},
        {NULL,              0,                  0,  0}
    };
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

    bool verbose = false;
    while (true) {
        int option_index = 0;
        int c = getopt_long (argc, argv, short_options, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'v':
                verbose = true;
                break;
            case 'c':
                log_warning ("--config-file switch not implemented yet (file: '%s')", optarg);
                break;
            case 'h':
            default:
                usage ();
                return EXIT_FAILURE;
        }
    }

    if (verbose)
        ManageFtyLog::getInstanceFtylog()->setVeboseMode();

    zactor_t *ms_server = zactor_new (fty_metric_store_server, (void *) NULL);
    if (!ms_server) {
        log_fatal ("zactor_new (task = 'fty_metric_store_server', args = 'NULL') failed");
        return EXIT_FAILURE;
    }

    zstr_sendx (ms_server, "CONNECT", ENDPOINT, AGENT_NAME, NULL);
    //zstr_sendx (ms_server, "CONSUMER", FTY_PROTO_STREAM_METRICS, ".*", NULL);
    zstr_sendx (ms_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);

    // setup the storage age
    for (int i = 0; i != STEPS_SIZE; i++) {
        const char *dfl = DEFAULTS [i];

        char *var_name = NULL;
        int r = asprintf (&var_name, "%s_%s", FTY_METRIC_STORE_CONF_PREFIX, STEPS[i]);
        assert (r != -1);
        if (var_name && getenv (var_name)) {
            dfl = getenv (var_name);
        }
        zstr_free (&var_name);

        zstr_sendx (ms_server, FTY_METRIC_STORE_CONF_PREFIX, STEPS [i], dfl, NULL);
    }

    log_info("fty_metric_store started");

    while (true) {
        char *message = zstr_recv (ms_server);
        if (message) {
            puts (message);
            zstr_free (&message);
        }
        else {
            puts ("interrupted");
            break;
        }
    }

    zactor_destroy (&ms_server);
    log_info("fty_metric_store end");

    return EXIT_SUCCESS;
}
