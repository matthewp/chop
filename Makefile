CC ?= cc
VERSION != git describe --tags --always --dirty 2>/dev/null || echo devel
VERSION := $(VERSION:v%=%)
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -g -DVERSION=\"$(VERSION)\"
LDFLAGS =
PREFIX ?= /usr/local

BIN = chop
OBJS = main.o chop.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN)

install: $(BIN)
	install -m 755 $(BIN) $(PREFIX)/bin/

.PHONY: all clean install
