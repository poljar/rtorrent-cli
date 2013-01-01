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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "util.h"
#include "scgi_proxy.h"

static int transport_write(int sockfd, char *buf, int len) {
    int n;

    assert(sockfd);
    assert(buf);
    assert(len);

    n = write(sockfd, buf, len);
    if (n < 0)
        return -1;

    return 0;
}

static int transport_read(int sockfd, char **buff) {
    int n, sum = 0;
    char tmp[1024];

    assert(*buff == NULL);

    while (1) {
        n = read(sockfd, tmp, 1024);

        if (n < 0)
            return -1;

        if (n == 0)
            break;

        sum += n;
        *buff = xrealloc(*buff, sum+1);
        strncpy(*buff+sum-n, tmp, n);
    }

    (*buff)[sum] = '\0';

    return sum;
}

static char *find_body(char *response) {
    char *body;
    assert(response);

    body = strstr(response, "\r\n\r\n");
    assert(body);

    return body+4;
}

static void prepare_header(char **header, int *header_length, int body_length) {
    char tmp[30];

    assert(header);
    assert(header_length);
    assert(body_length >= 0);

    *header_length = 23 + sprintf(tmp, "%d", body_length);
    *header = xmalloc(*header_length+30);
    sprintf(*header, "%d:CONTENT_LENGTH\n%d\nSCGI\n1\n,", *header_length, body_length);
    *header_length += 2 + sprintf(tmp, "%d", *header_length);

    for (int i = 0; i < *header_length; ++i)
        if ((*header)[i] == '\n')
            (*header)[i] = '\x00';
}

int scgi_create_transport(int *sockfd, const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int ret;

    assert(sockfd);
    assert(host);
    assert(port);

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    ret = getaddrinfo(host, port, &hints, &result);
    if (ret != 0)
        return ret;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        *sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (*sockfd == -1)
            continue;

       if (connect(*sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;

       close(*sockfd);
    }

    if (rp == NULL)
        ret = -2;
    freeaddrinfo(result);
    return ret;
}

/* FIXME UNIX socket support
int create_transportu(int *sockfd, const char *file) {
    return -2;
}
*/

/* FIXME the header and error checking is also important */
int scgi_make_call(int *sockfd, char *msg, int msg_length, char **body) {
    int header_length, ret;
    char *buff = NULL;
    char *header;

    assert(msg);
    assert(msg_length >= 0);

    prepare_header(&header, &header_length, msg_length);

    if ((ret = transport_write(*sockfd, header, header_length)) < 0)
        goto finish;
    if ((ret = transport_write(*sockfd, msg, msg_length)) < 0)
        goto finish;

    if ((ret = transport_read(*sockfd, &buff)) <= 0)
        goto finish;

    *body = xstrdup(find_body(buff));
    
finish:
    xfree((char*) header);
    xfree(buff);
    return ret;
}
