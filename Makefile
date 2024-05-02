CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
DFLAGS = -ggdb
RFLAGS = -Ofast
CLIBS = -fopenmp

CC_R = $(CC) $(CFLAGS) $(RFLAGS) $(CLIBS)
CC_D = $(CC) $(CFLAGS) $(DFLAGS) $(CLIBS)

.PHONY: all programs programs-debug clean format-workspace

all: programs programs-debug me/python/me.so
programs: me/me me/me-cli me/me-ascii-logger
programs-debug: me/me-debug me/me-cli me/me-ascii-logger

me/me: me/me.c me/me.h
	$(CC_R) -DME_BINARY me/me.c -o $@

me/me-debug: me/me.c me/me.h
	$(CC_D) -DME_BINARY me/me.c -o $@

me/me-cli: me/me.o me/me.h me/me-cli.c
	$(CC_R) me/me.c me/me-cli.c -o $@

me/me-ascii-logger: me/me.o me/me.h me/me-ascii-logger.c
	$(CC_R) me/me.c me/me-ascii-logger.c -o $@

me/me.o: me/me.c me/me.h
	$(CC_R) -fpic -c me/me.c -o $@

# We don't use the flags in the Python binding as the headers will pollute our
# compilation warnings.
me/python/me.so: me/me.o me/python/memodule.c
	$(CC) -pedantic -std=c99 $(RFLAGS) $(CLIBS) -shared -fpic $^ -o $@

clean:
	-rm me/me
	-rm me/me-debug
	-rm me/me-cli
	-rm me/me-ascii-logger
	-rm me/me.o
	-rm me/python/me.so

format-workspace:
	./format-workspace.sh
