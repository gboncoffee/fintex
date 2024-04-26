CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
DFLAGS = -ggdb
RFLAGS = -Ofast
CLIBS = -fopenmp

CC_R = $(CC) $(CFLAGS) $(RFLAGS) $(CLIBS)
CC_D = $(CC) $(CFLAGS) $(DFLAGS) $(CLIBS)

.PHONY: all programs clean

all: programs
programs: me/me me/me-debug me/me-cli me/me-ascii-logger

me/me: me/me.c me/me.h
	$(CC_R) -DME_BINARY me/me.c -o me/me

me/me-debug: me/me.c me/me.h
	$(CC_D) -DME_BINARY me/me.c -o me/me-debug

me/me-cli: me/me.c me/me.h me/me-cli.c
	$(CC_R) me/me.c me/me-cli.c -o me/me-cli

me/me-ascii-logger: me/me.c me/me.h me/me-ascii-logger.c
	$(CC_R) me/me.c me/me-ascii-logger.c -o me/me-ascii-logger

clean:
	-rm me/me
	-rm me/me-debug
	-rm me/me-cli
	-rm me/me-ascii-logger
