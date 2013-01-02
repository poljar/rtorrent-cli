/***
 * rtorrent-cli
 * Copyright (C) 2013 Damir Jelić
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "util.h"
#include "xmlrpc_client.h"

#define NAME "rtorrent-cli"
#define VERSION "0.1"

#define DEFAULT_SERVER "http://localhost/RPC2"

static char *server;
static char *port = "5000";
static xmlrpc_env env;

typedef struct {
    int64_t id;
    const char *hash;
    const char *name;
    bool complete;
    bool active;
    bool started;
    int64_t size_bytes;
    int64_t done_bytes;
    int64_t ratio;
    int64_t down_total;
    int64_t up_rate;
    int64_t down_rate;
} torrent_info;

typedef struct {
    torrent_info **torrents;
    size_t size;
} torrent_array;

static void torrent_info_free(torrent_info *info) {
    xfree((char*) info->name);
    xfree((char*) info->hash);
    xfree(info);
}

static torrent_array *torrent_array_new(size_t size) {
    torrent_array *array = xmalloc(sizeof(torrent_array));

    array->torrents = xmalloc(sizeof(torrent_info*)*size);

    for (size_t i = 0; i < size; ++i) {
        array->torrents[i] = xmalloc0(sizeof(torrent_info));
    }
    array->size = size;
    return array;
}

static void torrent_array_free(torrent_array *array) {
    for (size_t i = 0; i < array->size; ++i) {
        torrent_info_free(array->torrents[i]);
    }
    xfree(array->torrents);
    xfree(array);
}

static void usage() {
    printf ("usage bla \n");
    exit(0);
}

static void check_fault() {
    if (env.fault_occurred) {
        fprintf(stderr, "ERROR: %s (%d)\n", env.fault_string, env.fault_code);
        exit(1);
    }
}

static void parse_url(const char *urlArg, char **url) {
    if (strstr(urlArg, "://") != 0)
        *url = xstrdup(urlArg);
    else {
        *url = xmalloc(strlen(urlArg)+12);
        sprintf(*url, "http://%s/RPC2", urlArg);
    }
}

static void init() {
    xmlrpc_env_init(&env);

    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    check_fault(&env);
}

static void execute_method(xmlrpc_value **result, char *method, xmlrpc_value *params) {
    xmlrpc_server_info *serverInfo;

    serverInfo = xmlrpc_server_info_new(&env, server);
    *result = xmlrpc_client_call_server_params(&env, serverInfo, method, params);
    check_fault();
}

static void execute_proxy_method(xmlrpc_value **result, char *method, xmlrpc_value *params) {
    *result = xmlrpc_call_scgi_server_params(&env, server, port, method, params);
    check_fault();
}

static void get_string(xmlrpc_value *value, const char **string) {
    assert(xmlrpc_value_type(value) == XMLRPC_TYPE_STRING);
    xmlrpc_read_string(&env, value, string);
    check_fault();
}

static void get_int64(xmlrpc_value *value, int64_t *num) {
    xmlrpc_int64 tmp;
    assert(xmlrpc_value_type(value) == XMLRPC_TYPE_I8);
    xmlrpc_read_i8(&env, value, &tmp);
    check_fault();

    *num = (int64_t) tmp;
}

static void get_bool_from_int64(xmlrpc_value *value, bool *result) {
    int64_t tmp;
    get_int64(value, &tmp);
    *result = tmp == 1 ? true : false;
}

static void prepare_list_params(xmlrpc_value **params) {
    xmlrpc_value *tmp;
    const char **p;
    const char *arguments[] = { "main", "d.hash=", "d.name=", "d.is_active=",
                                "d.state=", "d.bytes_done=", "d.size_bytes=",
                                "d.up.rate=", "d.down.rate=", "d.down.total=",
                                "d.ratio=", "d.complete=", NULL };

    *params = xmlrpc_array_new(&env);
    check_fault();
    p = arguments;

    while (*p) {
        xmlrpc_array_append_item(&env, *params, tmp = xmlrpc_string_new(&env, *p));
        check_fault();
        xmlrpc_DECREF(tmp);
        p++;
    }
}

static void get_torrent_list(torrent_array **result) {
    size_t size;
    xmlrpc_value *xml_array, *params;

    prepare_list_params(&params);

    execute_proxy_method(&xml_array, "d.multicall", params);
    check_fault(&env);
    assert(xmlrpc_value_type(xml_array) == XMLRPC_TYPE_ARRAY);

    XMLRPC_ASSERT_ARRAY_OK(xml_array);
    size = xmlrpc_array_size(&env, xml_array);
    check_fault(&env);

    if (size <= 0)
        goto finish;
        
    *result = torrent_array_new(size);
    for (size_t i = 0; i < size; ++i) {
        size_t tarray_size;
        xmlrpc_value *tarray = NULL;

        xmlrpc_array_read_item(&env, xml_array, i, &tarray);
        check_fault(&env);
        assert(xmlrpc_value_type(tarray) == XMLRPC_TYPE_ARRAY);

        XMLRPC_ASSERT_ARRAY_OK(tarray);
        tarray_size = xmlrpc_array_size(&env, tarray);

        (*result)->torrents[i]->id = i+1;
        for (size_t j = 0; j < tarray_size; ++j) {
            xmlrpc_value *item = NULL;

            xmlrpc_array_read_item(&env, tarray, j, &item);
            check_fault(&env);

            switch (j) {
                case 0:
                    get_string(item, &(*result)->torrents[i]->hash);
                    break;
                case 1:
                    get_string(item, &(*result)->torrents[i]->name);
                    break;
                case 2:
                    get_bool_from_int64(item, &(*result)->torrents[i]->active);
                    break;
                case 3:
                    get_bool_from_int64(item, &(*result)->torrents[i]->started);
                    break;
                case 4:
                    get_int64(item, &(*result)->torrents[i]->done_bytes);
                    break;
                case 5:
                    get_int64(item, &(*result)->torrents[i]->size_bytes);
                    break;
                case 6:
                    get_int64(item, &(*result)->torrents[i]->up_rate);
                    break;
                case 7:
                    get_int64(item, &(*result)->torrents[i]->down_rate);
                    break;
                case 8:
                    get_int64(item, &(*result)->torrents[i]->down_total);
                    break;
                case 9:
                    get_int64(item, &(*result)->torrents[i]->ratio);
                    break;
                case 10:
                    get_bool_from_int64(item, &(*result)->torrents[i]->complete);
                    break;
                default:
                    ;
            }
            xmlrpc_DECREF(item);
        }
        xmlrpc_DECREF(tarray);
    }

finish:
    xmlrpc_DECREF(xml_array);
}

static void eta_to_string(char *buf, size_t  buflen, int64_t eta) {
    if (eta < 0)
        snprintf (buf, buflen, "Unknown");
    else if (eta < 60)
        snprintf (buf, buflen, "%" PRId64 " sec", eta);
    else if (eta < (60 * 60))
        snprintf (buf, buflen, "%.1f min", (double) eta / 60);
    else if (eta < (60 * 60 * 24))
        snprintf (buf, buflen, "%.1f hrs", (double) eta / (60 * 60));
    else if (eta < (60 * 60 * 24 * 365))
        snprintf (buf, buflen, "%1d days", (int) eta / (60 * 60 * 24));
    else
        snprintf (buf, buflen, "Inf");
}

static void byte_to_string(char *buf, size_t buflen, int64_t byte) {
    if (byte < 1024)
        snprintf (buf, buflen, "%" PRId64 "B", byte);
    else if (byte < 1048576)
        snprintf (buf, buflen, "%.1fKiB", (double) byte / 1024);
    else if (byte < 1073741824)
        snprintf (buf, buflen, "%.1fMiB", (double) byte / 1048576);
    else if (byte < 1099511627776)
        snprintf (buf, buflen, "%.1fGiB", (double) byte / 1073741824);
    else if (byte < 1125899906842624)
        snprintf (buf, buflen, "%.1fTiB", (double) byte / 1099511627776);
    else
        snprintf (buf, buflen, "Inf");
}

static void print_torrent(torrent_info *info) {
    int done;
    int64_t eta;
    double ratio;
    char status[20], etastr[20], up_rate[20], down_rate[20], have[20];

    done = (float) info->done_bytes / (float) info->size_bytes * 100;
    ratio = (double) info->ratio / 1000;
    eta = info->down_rate == 0 ? -1 : info->size_bytes / info->down_rate;
    byte_to_string(down_rate, 20, info->down_rate);
    byte_to_string(up_rate, 20, info->up_rate);
    byte_to_string(have, 20, info->done_bytes);

    if (info->complete)
        sprintf(etastr, "Done");
    else
        eta_to_string(etastr, 20, eta);

    if (!info->active)
        strcpy(status, "Stoped");
    else if (info->up_rate == 0 && info->down_rate == 0)
        strcpy(status, "Idle");
    else
        strcpy(status, "Active");

    printf("%4" PRId64 ". %4d%% %9s  %-8s  %8s  %8s %8.2f   %-11s  %s\n", 
           info->id, done, have, etastr, up_rate, down_rate,
           ratio, status, info->name);
}

static void list_torrents()
{
    int64_t total_size = 0, total_up = 0, total_down = 0;
    char sizestr[20], upstr[20], downstr[20];
    torrent_array *tarray = NULL;

    get_torrent_list(&tarray);
    if (tarray == NULL)
        return;

    printf ("%-4s   %-4s  %8s  %-8s  %8s  %8s %9s  %-11s  %s\n", 
            "ID", "Done", "Have", "ETA", "Up", "Down", "Ratio", "Status", "Name");
    for (size_t i = 0; i < tarray->size; ++i) {
        print_torrent(tarray->torrents[i]);
        total_size += tarray->torrents[i]->done_bytes;
        total_up += tarray->torrents[i]->up_rate;
        total_down += tarray->torrents[i]->down_rate;
    }

    byte_to_string(sizestr, 20, total_size);
    byte_to_string(upstr, 20, total_up);
    byte_to_string(downstr, 20, total_down);
    printf ("Sum:        %9s            %8s  %8s\n",
            sizestr, upstr, downstr);

    torrent_array_free(tarray);
}

int main(int argc, char *argv[])
{
    static const struct option opts[] = {
        { "list", no_argument, 0, 'l' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 },
    };

    if (argc < 2) {
        usage();
        return 0;
    }

    if (*argv[1] != '-') {
        if (argc < 3) {
            usage();
            return 0;
        }

        //parse_url(argv[1], &server);
        server = xstrdup(argv[1]);
        argc--;
        for (int i = 1; i < argc; i++)
            argv[i] = argv[i+1];
    } else
        server = xstrdup(DEFAULT_SERVER);

    for (;;) {
        int opt = getopt_long(argc, argv, "lh", opts, NULL);
        if (opt == -1)
            break;

        switch (opt) {
            case 'h':
                usage();
            case 'l':
                list_torrents();
                break;
            default:
                usage();
        }
    }
    xfree(server);
    xmlrpc_env_clean(&env);

    return 0;
}
