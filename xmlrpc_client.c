/***
* This file is part of rtorrent-cli
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

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

#include "util.h"
#include "xmlrpc_client.h"
#include "scgi_proxy.h"

static void prepare_xml(xmlrpc_env *env, xmlrpc_mem_block **memblock, const char *method, xmlrpc_value *param) {
    XMLRPC_ASSERT_ENV_OK(env);
    assert(method);
    assert(param);

    *memblock = XMLRPC_MEMBLOCK_NEW(char, env, 0);

    if (env->fault_occurred)
        return;

    xmlrpc_serialize_call(env, *memblock, method, param);
}

static void parse_xml(xmlrpc_env *env, char *xml, int xml_size, xmlrpc_value **result, int *fault_code, const char **fault_string) {
    xmlrpc_env respEnv;

    XMLRPC_ASSERT_ENV_OK(env);

    xmlrpc_env_init(&respEnv);

    xmlrpc_parse_response2(&respEnv, xml, xml_size, result, fault_code, fault_string);

    if (respEnv.fault_occurred) {
        xmlrpc_env_set_fault_formatted(env, respEnv.fault_code,
               "Unable to make sense of XML-RPC response from server. " 
               "%s. Use XMLRPC_TRACE_XML to see for yourself",
               respEnv.fault_string);
    }

    xmlrpc_env_clean(&respEnv);
}

xmlrpc_value *xmlrpc_call_scgi_server_params(xmlrpc_env *env, const char *server, const char *port, const char *method, xmlrpc_value *param) {
    int sockfd, fault_code;
    char *buff = NULL;
    xmlrpc_value *result = NULL;
    const char *fault_string = NULL;
    xmlrpc_mem_block *memblock = NULL;

    XMLRPC_ASSERT_ENV_OK(env);

    assert(server);
    assert(port);
    assert(method);
    assert(param);

    prepare_xml(env, &memblock, method, param);
    if (env->fault_occurred)
        goto finish;

    xmlrpc_DECREF(param);

    if (scgi_create_transport(&sockfd, server, port) < 0) {
        xmlrpc_env_set_fault_formatted(env, -32300, "Could not connect");
        goto finish;
    }

    scgi_make_call(&sockfd, XMLRPC_MEMBLOCK_CONTENTS(char, memblock), XMLRPC_MEMBLOCK_SIZE(char, memblock), &buff);

    /*
    printf("%s\n", buff);
    */

    parse_xml(env, buff, strlen(buff), &result, &fault_code, &fault_string);
        
finish:
    close(sockfd);

    xfree(buff);
    xfree((char*) fault_string);
    XMLRPC_MEMBLOCK_FREE(char, memblock);

    return result;
}
