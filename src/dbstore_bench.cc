/*
 *
 * Copyright (C) 2016 Eaton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/*!
 * \file dbstore_bench.cc
 * \author Gerald Guillaume <GeraldGuillaume@Eaton.com>
 * \brief do intensive and endurance insertion job
 */
#include <getopt.h>
#include <ctime>
#include "fty_metric_store_classes.h"

using namespace std;

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



long get_clock_ms(){
    struct timeval time;
    gettimeofday(&time, NULL); // Get Time
    return (time.tv_sec * 1000) + (time.tv_usec / 1000);
}

char clock_fmt[26];
char *get_clock_fmt(){
    time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(clock_fmt, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return clock_fmt;
}

/* Insert one measurement on a random device, a random topic and a random value
 */
void insert_new_measurement(
        int device_id,
        int topic_id,
        tntdb::Connection conn
        //persist::TopicCache &cache,
        //persist::MultiRowCache &multi_row
){
    char topic_name[32];
    char device_name[32];

    sprintf(device_name,"bench.asset%d",device_id);
    sprintf(topic_name,"bench.topic%d@%s",topic_id,device_name);

    insert_into_measurement(
            conn, topic_name, rand() % 999999, 0, time(NULL),
            "%", device_name);

}
/*
 * do the bench insertion
 * \param delay       - pause between each insertion (in ms), 0 means no delay
 * \param num_device  - number of simulated device
 * \param topic_per_device  - number of topic per device simulated
 * \param total_duration    - bench duration in minute, -1 means infinite loop
 * \param insertion         - number of maximum row before inserting in multi-row
 * \param periodic_display  - Each periodic_display seconds, output
 *  time; total rows; row over since periodic_display s  ; average since periodic_display s
 */
void bench(
        int delay=100,
        int num_device=100,
        int topic_per_device=100,
        int periodic_display=10,
        int total_duration=-1,
        int insertion=10){

    zsys_info("delay=%dms periodic=%ds minute=%dm element=%d topic=%d insert_every=%d",
            delay,periodic_display,total_duration,num_device,topic_per_device,insertion);
    tntdb::Connection conn;
    try {
        conn = tntdb::connectCached(url);
        conn.ping();
    } catch (const std::exception &e) {
        zsys_error("Can't connect to the database");
        return;
    }

    int stat_total_row=0;
    int stat_periodic_row=0;

    zsys_catch_interrupts ();

    long begin_overall_ms = get_clock_ms();
    long begin_periodic_ms = get_clock_ms();

    int dev_by_topic=num_device * topic_per_device;

    zsys_info("time;total;rows; mean over last %ds (row/s)",periodic_display);
    while(!zsys_interrupted) {
        insert_new_measurement(stat_total_row%dev_by_topic/topic_per_device, stat_total_row%topic_per_device,
                conn);
        //count stat
        stat_total_row++;
        stat_periodic_row++;
        //time to display stat ?
        long now_ms = get_clock_ms();
        long elapsed_periodic_ms = (now_ms - begin_periodic_ms);
        //every period seconds display current total row count and the trend over the last periodic_display second
        if(elapsed_periodic_ms > periodic_display * 1000 ){
            zsys_info("%s;%d;%d;%.2lf",get_clock_fmt(),stat_total_row,stat_periodic_row,stat_periodic_row/(elapsed_periodic_ms/1000.0));
            stat_periodic_row=0;
            begin_periodic_ms = now_ms;
        }
        if (total_duration>0 && (now_ms - begin_overall_ms)/1000.0/60.0>total_duration)goto exit;

        //sleep before loop
        if(delay>0)usleep(delay*1000);
    }

exit:
    flush_measurement(url);
    long elapsed_overall_ms = (get_clock_ms() - begin_overall_ms);

    zsys_info("%d rows inserted in  %.2lf seconds, overall avg=%.2lf row/s",stat_total_row,elapsed_overall_ms/1000.0,stat_total_row/(elapsed_overall_ms/1000.0));
}

void usage ()
{
    puts ("dbstore_bench [options] \n"
          "  -u|--url              mysql:db=box_utf8;user=bios;password=test (or set DB_PASSWD and DB_USER env variable)\n"
          "  -d|--delay            pause between each insertion (in ms), 0 means no delay [100]\n"
          "  -p|--periodic         output time; row; average each periodic_display seconds [10]\n"
          "  -m|--minute           bench duration in minute, -1 means infinite loop [-1]\n"
          "  -e|--element          number of simulated elements [100]\n"
          "  -t|--topic            number of simulated topic per element [100]\n"
          "  -i|--insert_every     do a multi row insertion on every X measurement[10]\n"
          "  -h|--help             print this information");
}

/*
 *
 */
int main(int argc, char** argv) {
    // set default
    int help = 0;
    int delay=100; //ms
    int periodic=10; //s
    int minute=-1; //min
    int element=100;
    int topic=100;
    int insert_every=10;

     // get options
    int c;
    static struct option long_options[] =
    {
            {"help",       no_argument,       &help,    1},
            {"url",        required_argument, 0,'u'},
            {"delay",      required_argument, 0,'d'},
            {"periodic",   required_argument, 0,'p'},
            {"minute",     required_argument, 0,'m'},
            {"element",    required_argument, 0,'e'},
            {"topic",      required_argument, 0,'t'},
            {"insert_every",  required_argument, 0,'i'},
            {NULL, 0, 0, 0}
    };

    while(true) {
        int option_index = 0;
        c = getopt_long (argc, argv, "h:u:d:p:m:e:t:i:", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
        case 'u':
            url = optarg;
            break;
        case 'd':
            delay = atoi(optarg);
            break;
        case 'p':
            periodic = atoi(optarg);
            break;
        case 'm':
            minute = atoi(optarg);
            break;
        case 'e':
            element = atoi(optarg);
            break;
        case 't':
            topic = atoi(optarg);
            break;
        case 'i':
            insert_every = atoi(optarg);
            break;
        case 0:
            // just now walking trough some long opt
            break;
        case 'h':
        default:
            help = 1;
            break;
        }
    }
    if (help) { usage(); exit(1); }

    zsys_debug("## bench started ##");

    bench(delay,element, topic,  periodic, minute, insert_every);
    return 0;
}
