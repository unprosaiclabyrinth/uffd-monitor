CC            := gcc
CFLAGS        := -Wall -Wextra -shared -fPIC
CFLAGS_COMPEL := -Wall -Werror -O2 -g
COMPEL        := ./criu-3.19/compel/compel-host

all: libuffd.so

run1: libuffd.so
	sudo LD_PRELOAD=./$< ./test/t01

run2: libuffd.so
	sudo LD_PRELOAD=./$< ./test/t02

run3: libuffd.so
	sudo LD_PRELOAD=./$< ./test/t03

run4: libuffd.so
	sudo LD_PRELOAD=./$< ./test/t04

run5: libuffd.so
	sudo LD_PRELOAD=./$< ./test/t05/lighttpd-1.4.74/src/lighttpd \
		-f ./test/t05/lighttpd-custom.conf -D
	
run6: libuffd.so
	sudo LD_PRELOAD=./$< ./test/t06/t06

libuffd.so: uffd.c vma.c fork.c queue.c log.c sigchld.c spy.c clean test
	$(CC) $(CFLAGS) $(CFLAGS_COMPEL) $(shell $(COMPEL) includes) $< vma.c fork.c queue.c log.c sigchld.c -o $@ spy.c $(shell $(COMPEL) --static libs)

test:
	make -C test

clean:
	@rm -f libuffd.so
	make -C test clean

.PHONY: all run1 run2 run3 run4 run5 run6 test clean
