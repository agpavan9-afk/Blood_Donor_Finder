# Makefile for BloodLink — Blood Donor Finder
# Works on Windows (Git Bash / WSL / MinGW) and Linux/macOS

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
LDFLAGS = -lm

CORE_SRCS = core/rbtree.c core/skiplist.c core/matcher.c
CLI_SRC   = core/matcher_cli.c
MAIN_SRC  = main.c

.PHONY: all demo cli clean

all: demo cli

demo: $(MAIN_SRC) $(CORE_SRCS) core/rbtree.h core/skiplist.h core/matcher.h
	$(CC) $(CFLAGS) $(MAIN_SRC) core/rbtree.c core/skiplist.c $(LDFLAGS) -o blood_donor_finder

cli: $(CORE_SRCS) $(CLI_SRC) core/rbtree.h core/skiplist.h core/matcher.h
	$(CC) $(CFLAGS) $(CORE_SRCS) $(CLI_SRC) $(LDFLAGS) -o core/bdf_matcher_cli

clean:
	rm -f blood_donor_finder blood_donor_finder.exe
	rm -f core/bdf_matcher_cli core/bdf_matcher_cli.exe
