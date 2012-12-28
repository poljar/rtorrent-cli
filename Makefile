CC=gcc
CFLAGS := -Wall -Wextra -pedantic -std=c99 -g $(CFLAGS)
LDLIBS := -lxmlrpc -lxmlrpc_util -lxmlrpc_client

rtorrent-cli: rtorrent-cli.o

clean:
	$(RM) rtorrent-cli rtorrent-cli.o
