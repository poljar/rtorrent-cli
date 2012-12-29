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

#define NAME "rtorrent-cli"
#define VERSION "0.1"

#define DEFAULT_SERVER "http://localhost/RPC2"

static char *server;
static xmlrpc_env env;

typedef struct {
    int64_t id;
    char hash[41];
    char *name;
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

static void *xmalloc(size_t size) {
    void *p;
    assert(size > 0);

    if (!(p = malloc(size))) {
        fprintf(stderr, "Not enough memory!\n");
        raise(SIGQUIT);
    }

    return p;
}

void *xmalloc0(size_t size) {
    void *p;
    assert(size > 0);

    if (!(p = calloc(1, size))) {
        fprintf(stderr, "Not enough memory!\n");
        raise(SIGQUIT);
    }

    return p;
}

static void *xmemdup(const void *p, size_t l) {
    if (!p)
        return NULL;
    else {
        char *r = xmalloc(l);
        memcpy(r, p, l);
        return r;
    }
}

static char *xstrdup(const char *s) {
    if (!s)
        return NULL;

    return xmemdup(s, strlen(s)+1);
}

static void xfree(void *p) {
    if (!p)
        return;

    free(p);
}

static void torrent_info_free(torrent_info *info) {
    xfree(info->name);
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

static void check_fault(xmlrpc_env *env) {
    if (env->fault_occurred) {
        fprintf(stderr, "ERROR: %s (%d)\n", env->fault_string, env->fault_code);
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

static void execute_method(xmlrpc_env *env, xmlrpc_value **result, char *method, xmlrpc_value *params) {
    xmlrpc_server_info *serverInfo;

    serverInfo = xmlrpc_server_info_new(env, server);
    *result = xmlrpc_client_call_server_params(env, serverInfo, method, params);
    check_fault(env);
}

static void get_torrent_name(const char *hash, const char **namestring) {
    xmlrpc_value *name, *params;
    const char *tmp;

    params = xmlrpc_array_new(&env);
    xmlrpc_array_append_item(&env, params, xmlrpc_string_new(&env, hash));
    execute_method(&env, &name, "d.name", params);

    assert(xmlrpc_value_type(name) == XMLRPC_TYPE_STRING);

    xmlrpc_read_string(&env, name, &tmp);
    *namestring = xstrdup(tmp);

    xfree((char*) tmp);
    xmlrpc_DECREF(name);
}

static void get_torrent_int64(const char *hash, char *method, int64_t *value) {
    xmlrpc_value *xmlvalue, *params;
    xmlrpc_int64 tmp;

    params = xmlrpc_array_new(&env);
    xmlrpc_array_append_item(&env, params, xmlrpc_string_new(&env, hash));
    execute_method(&env, &xmlvalue, method, params);

    assert(xmlrpc_value_type(xmlvalue) == XMLRPC_TYPE_I8);

    xmlrpc_read_i8(&env, xmlvalue, &tmp);

    *value = (int64_t) tmp;
}

static void get_torrent_info(const char *hash, torrent_info *info) {
    const char *name;
    int64_t tmp;

    get_torrent_name(hash, &name);
    info->name = xstrdup(name);

    get_torrent_int64(hash, "d.complete", &tmp);
    info->complete = tmp == 1 ? true : false;

    get_torrent_int64(hash, "d.is_active", &tmp);
    info->active = tmp == 1 ? true : false;

    get_torrent_int64(hash, "d.state", &tmp);
    info->started = tmp == 1 ? true : false;

    get_torrent_int64(hash, "d.bytes_done", &info->done_bytes);
    get_torrent_int64(hash, "d.size_bytes", &info->size_bytes);
    get_torrent_int64(hash, "d.up.rate", &info->up_rate);
    get_torrent_int64(hash, "d.down.rate", &info->down_rate);
    get_torrent_int64(hash, "d.down.total", &info->down_total);
    get_torrent_int64(hash, "d.ratio", &info->ratio);

    xfree((char*) name);
}

static void get_torrent_list(torrent_array **tarray) {
    unsigned int size;
    xmlrpc_value *xml_array, *params;

    params = xmlrpc_array_new(&env);
    xmlrpc_array_append_item(&env, params, xmlrpc_nil_new(&env));
    execute_method(&env, &xml_array, "download_list", params);

    assert(xmlrpc_value_type(xml_array) == XMLRPC_TYPE_ARRAY);

    XMLRPC_ASSERT_ARRAY_OK(xml_array);
    size = xmlrpc_array_size(&env, xml_array);
    check_fault(&env);

    (*tarray) = torrent_array_new(size);
    for (size_t i = 0; i < size; ++i) {
        size_t length;
        const char *hash;
        xmlrpc_value *item;

        xmlrpc_array_read_item(&env, xml_array, i, &item);
        check_fault(&env);

        xmlrpc_read_string_lp(&env, item, &length, &hash);
        check_fault(&env);

        (*tarray)->torrents[i]->id = i+1;
        strncpy((*tarray)->torrents[i]->hash, hash, 40);
        get_torrent_info(hash, (*tarray)->torrents[i]);

        xmlrpc_DECREF(item);
        xfree((char*) hash);
    }
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
    torrent_array *tarray;

    get_torrent_list(&tarray);

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

        parse_url(argv[1], &server);
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
                init();
                list_torrents(server);
                xmlrpc_client_cleanup();
                break;
            default:
                usage();
        }
    }
    xfree(server);
    xmlrpc_env_clean(&env);

    return 0;
}
