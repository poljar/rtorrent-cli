#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#define NAME "rtorrent-cli"
#define VERSION "0.1"

#define DEFAULT_SERVER "http://localhost/RPC2"

static char *server;

static void *xmalloc(size_t size) {
    void *p;

    if (!(p = malloc(size))) {
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

void xfree(void *p) {
    if (!p)
        return;

    free(p);
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

static void init(xmlrpc_env *env) {
    xmlrpc_env_init(env);

    xmlrpc_client_init2(env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    check_fault(env);
}


static void execute_method(xmlrpc_env *env, xmlrpc_value **result, char *method, xmlrpc_value *params) {
    xmlrpc_server_info *serverInfo;

    serverInfo = xmlrpc_server_info_new(env, server);
    *result = xmlrpc_client_call_server_params(env, serverInfo, method, params);
    check_fault(env);
}

static void get_torrent_name(xmlrpc_env *env, const char *hash, const char **namestring) {
    xmlrpc_value *name, *params;
    const char *tmp;

    params = xmlrpc_array_new(env);
    xmlrpc_array_append_item(env, params, xmlrpc_string_new(env, hash));
    execute_method(env, &name, "d.get_name", params);

    if (xmlrpc_value_type(name) != XMLRPC_TYPE_STRING) {
        printf("Unknown type: bailing\n");
        return;
    }
    xmlrpc_read_string(env, name, &tmp);
    *namestring = xmalloc(61);
    snprintf((char *)*namestring, 60, "%s", tmp);

    xfree((char*) tmp);
    xmlrpc_DECREF(name);
}

/* FIXME create a torrent struct, find the info and print it out somewhere else */
static void parse_torrents(xmlrpc_env *env, xmlrpc_value *array) {
    unsigned int size;

    if (xmlrpc_value_type(array) != XMLRPC_TYPE_ARRAY) {
        printf("Unknown type: bailing\n");
        return;
    }

    XMLRPC_ASSERT_ARRAY_OK(array);
    size = xmlrpc_array_size(env, array);
    check_fault(env);

    for (size_t i = 0; i < size; ++i) {
        size_t length;
        const char *hash, *name;
        char *spacing = "    ";
        xmlrpc_value *item;

        xmlrpc_array_read_item(env, array, i, &item);
        check_fault(env);

        xmlrpc_read_string_lp(env, item, &length, &hash);
        check_fault(env);

        /* FIXME This is only usefull for torrent lists < 100 */
        spacing = i >= 9 ? "" : " ";
        printf("%lu. %s%s", i+1, spacing, hash);
        get_torrent_name(env, hash, &name);
        printf("  %s\n", name);

        xfree((char*) name);
        xmlrpc_DECREF(item);
        xfree((char*) hash);
    }
}

static void list_torrents()
{
    xmlrpc_env env;
    xmlrpc_value *torrentlist, *params;

    init(&env);

    params = xmlrpc_array_new(&env);
    xmlrpc_array_append_item(&env, params, xmlrpc_nil_new(&env));
    execute_method(&env, &torrentlist, "download_list", params);

    parse_torrents(&env, torrentlist);

    xmlrpc_DECREF(torrentlist);
    xmlrpc_env_clean(&env);
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
                list_torrents(server);
                break;
            default:
                usage();
        }
    }
    xfree(server);
    xmlrpc_client_cleanup();

    return 0;
}
