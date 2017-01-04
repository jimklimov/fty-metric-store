/*  =========================================================================
    fty_metric_store - Metric store agent

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
    fty_metric_store - Metric store agent
@discuss
@end
*/

#include "fty_metric_store_classes.h"

#include <getopt.h>

static const char *AGENT_NAME = "fty-metric-store";
static const char *ENDPOINT = "ipc://@/malamute";
#define STEPS_SIZE 8
static const char *STEPS[STEPS_SIZE] = {"RT", "15m", "30m", "1h", "8h", "1d", "7d", "30d"};
static const char *DEFAULTS[STEPS_SIZE] = {"0", "1", "1",   "7",  "7",  "30", "30", "180"};

#define str(x) #x

#define DEFAULT_LOG_LEVEL LOG_WARNING

void usage () {
    puts ("fty-metric-store [options] ...\n"
          "  --log-level / -l       bios log level\n"
          "                         overrides setting in env. variable FTY_LOG_LEVEL\n"
          "  --config-file / -c     TODO\n"
          "  --help / -h            this information\n"
          );
}

int get_log_level (const char *level) {
    if (streq (level, str(LOG_DEBUG))) {
        return LOG_DEBUG;
    }
    else
    if (streq (level, str(LOG_INFO))) {
        return LOG_INFO;
    }
    else
    if (streq (level, str(LOG_WARNING))) {
        return LOG_WARNING;
    }
    else
    if (streq (level, str(LOG_ERR))) {
        return LOG_ERR;
    }
    else
    if (streq (level, str(LOG_CRIT))) {
        return LOG_CRIT;
    }
    return -1;
}

int main (int argc, char *argv [])
{
    int help = 0;
    int log_level = -1;
    char *config_file = NULL;

    while (true) {
        static struct option long_options[] =
        {
            {"help",            no_argument,        0,  1},
            {"log-level",       required_argument,  0,  'l'},
            {"config-file",     required_argument,  0,  'c'},
            {0,                 0,                  0,  0}
        };

        int option_index = 0;
        int c = getopt_long (argc, argv, "hl:c:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 'l':
            {
                log_level = get_log_level (optarg);
                break;
            }
            case 'c':
            {
                config_file = optarg;
                log_warning ("--config-file switch no implemented yet '%s'", config_file);
                break;
            }
            case 'h':
            default:
            {
                help = 1;
                break;
            }
        }
    }
    if (help) {
        usage ();
        return EXIT_FAILURE;
    }
    // log_level cascade (priority ascending)
    //  1. default value
    //  2. env. variable
    //  3. command line argument
    //  4. actor message - NOT IMPLEMENTED YET
    if (log_level == -1) {
        char *env_log_level = getenv ("FTY_LOG_LEVEL");
        if (env_log_level) {
            log_level = get_log_level (env_log_level);
            if (log_level == -1)
                log_level = DEFAULT_LOG_LEVEL;
        }
        else {
            log_level = DEFAULT_LOG_LEVEL;
        }
    }
    log_set_level (log_level);

    zactor_t *ms_server = zactor_new (fty_metric_store_server, (void *) NULL);
    if (!ms_server) {
        log_critical ("zactor_new (task = 'fty_metric_store_server', args = 'NULL') failed");
        return EXIT_FAILURE;
    }
    zstr_sendx (ms_server, "CONNECT", ENDPOINT, AGENT_NAME, NULL);
    zstr_sendx (ms_server, "CONSUMER", FTY_PROTO_STREAM_METRICS, ".*", NULL);
    zstr_sendx (ms_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);

    // setup the storage age
    for (int i = 0; i != STEPS_SIZE; i++) {
        char *env;
        const char *dfl = DEFAULTS [i];
        int r = asprintf (&env, "%s_%s", FTY_METRIC_STORE_CONF_PREFIX, STEPS[i]);
        assert (r != -1);

        if (getenv (env)) {
            dfl = getenv (env);
        }

        zstr_sendx (ms_server, FTY_METRIC_STORE_CONF_PREFIX, STEPS [i], dfl, NULL);

        zstr_free (&env);
    }

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
    return EXIT_SUCCESS;
}
