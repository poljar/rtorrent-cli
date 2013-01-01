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

#ifndef utilh
#define utilh

#include <stdlib.h>

void *xmalloc(size_t size);
void *xmalloc0(size_t size);
void *xrealloc(void *ptr, size_t size);
void *xmemdup(const void *p, size_t l);
char *xstrdup(const char *s);
void xfree(void *p);

void error(const char *msg);


#endif
