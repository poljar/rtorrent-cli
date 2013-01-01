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

#ifndef xmlclient
#define xmlclient

#include <xmlrpc-c/base.h>
xmlrpc_value *xmlrpc_call_scgi_server_params(xmlrpc_env *env, const char *server, const char *port, const char *method, xmlrpc_value *param);

#endif
