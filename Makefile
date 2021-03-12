# University of California, Irvine
# ICS 53
# Author: Liwei Lu and Qiwei He
# This Makefile is able to compile all c file in the src folder
# and put the ELF file in the build folder.

# Use gcc as a compiler
CC=gcc
CFLAGS= -pthread -g -fprofile-arcs -ftest-coverage -Wall -Wextra
SOURCES := $(wildcard */*.c)
REMOVE := $(patsubst %.c, %, $(SOURCES))
EXECUTABLES := $(patsubst %.c, %, $(SOURCES))
PORT=30000

.PHONY: all clean test
.PRECIOUS: build/% 

all: $(EXECUTABLES)
	@echo Done!

%: %.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	@rm -rf $(REMOVE)
	@rm -rf $(wildcard *.gcda) $(wildcard *.gcno) $(wildcard core*)
	@echo Clean All!
	@echo "123456" > a.txt
	@echo "abcd" > b.txt
	@echo "efgh" > c.txt

runs:
	sh Server.sh

runc:
	sh Client.sh

gdbs:
	sh gdbServer.sh

gdbc:
	sh gdbClient.sh