CC     = gcc
CFLAGS = -Wall -Wextra -shared -fPIC

all: libuffd.so

run1: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t01

run2: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t02

run3: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t03

run4: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t04

libuffd.so: uffd.c vma.c fork.c poll.c log.c sigsegv.c clean test
	$(CC) $(CFLAGS) $< vma.c fork.c poll.c log.c sigsegv.c -o $@

test:
	make -C test

clean:
	@rm -f libuffd.so
	make -C test clean

.PHONY: all run1 run2 run3 run4 test clean