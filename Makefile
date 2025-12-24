CC ?= cc
VERSION != git describe --tags --always --dirty 2>/dev/null || echo devel
VERSION := $(VERSION:v%=%)
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -g -DVERSION=\"$(VERSION)\"
LDFLAGS =
PREFIX ?= /usr/local

BIN = chop
OBJS = main.o chop.o
MAN = chop.1

all: $(BIN) $(MAN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

$(MAN): chop.1.scd
	scdoc < chop.1.scd > $@

clean:
	rm -f $(OBJS) $(BIN) $(MAN)

install: $(BIN) $(MAN)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/
	install -m 644 $(MAN) $(DESTDIR)$(PREFIX)/share/man/man1/

.PHONY: all clean install
