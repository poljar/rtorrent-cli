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

#include "util.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void *xmalloc(size_t size) {
    void *p;
    assert(size > 0);

    if (!(p = malloc(size))) {
        fprintf(stderr, "Not enough memory!\n");
        exit(EXIT_FAILURE);
    }

    return p;
}

void *xmalloc0(size_t size) {
    void *p;
    assert(size > 0);

    if (!(p = calloc(1, size))) {
        fprintf(stderr, "Not enough memory!\n");
        exit(EXIT_FAILURE);
    }

    return p;
}

void *xrealloc(void *ptr, size_t size) {
    void *p;
    assert(size > 0);

    if (!(p = realloc(ptr, size))) {
        fprintf(stderr, "Not enough memory!\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xmemdup(const void *p, size_t l) {
    if (!p)
        return NULL;
    else {
        char *r = xmalloc(l);
        memcpy(r, p, l);
        return r;
    }
}

char *xstrdup(const char *s) {
    if (!s)
        return NULL;

    return xmemdup(s, strlen(s)+1);
}

void xfree(void *p) {
    if (!p)
        return;

    free(p);
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void assert_not_reached() {
    printf("Code should not be reached at %s:%u, function %s(). Aborting.", __FILE__, __LINE__, __func__);
    abort();
}
