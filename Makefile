# Makefile for BloodLink — Blood Donor Finder
# Works with GNU Make and a C11 GCC-compatible compiler.

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS = -lm

DEMO_SRCS    = main.c core/rbtree.c core/skiplist.c
MATCHER_SRCS = core/matcher.c core/matcher_cli.c

.PHONY: all demo cli clean

all: demo cli

demo: $(DEMO_SRCS) core/rbtree.h core/skiplist.h
	$(CC) $(CFLAGS) $(DEMO_SRCS) $(LDFLAGS) -o blood_donor_finder

cli: $(MATCHER_SRCS) core/matcher.h
	$(CC) $(CFLAGS) $(MATCHER_SRCS) $(LDFLAGS) -o core/bdf_matcher_cli

clean:
	rm -f blood_donor_finder blood_donor_finder.exe
	rm -f core/bdf_matcher_cli core/bdf_matcher_cli.exe
