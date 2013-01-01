CC=gcc
CFLAGS := -Wall -Wextra -pedantic -std=gnu99 -g $(CFLAGS)
LDLIBS := -lxmlrpc -lxmlrpc_util -lxmlrpc_client

SRC = util.c scgi_proxy.c xmlrpc_client.c rtorrent-cli.c
OBJ = ${SRC:.c=.o}
.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

rtorrent-cli: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDLIBS}
clean:
	@echo cleaning
	$(RM) rtorrent-cli ${OBJ}

.PHONY: all clean
