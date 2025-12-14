CC = gcc
VERSION := $(shell (git describe --tags --always --dirty 2>/dev/null || echo "devel") | sed 's/^v//')
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -g -DVERSION=\"$(VERSION)\"
LDFLAGS =

SRC = main.c chop.c
OBJ = $(SRC:.c=.o)
BIN = chop

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	cp $(BIN) /usr/local/bin/

.PHONY: all clean install
