CC            := gcc
CFLAGS        := -Wall -Wextra -shared -fPIC
CFLAGS_COMPEL := -Wall -Werror -O2 -g
COMPEL        := ./criu-3.19/compel/compel-host

all: libuffd.so

run1: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t01

run2: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t02

run3: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t03

run4: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t04

run5: libuffd.so
	sudo LD_PRELOAD=./libuffd.so ./test/t05/lighttpd-1.4.74/src/lighttpd \
		-f ./test/t05/lighttpd-custom.conf -D

libuffd.so: uffd.c vma.c fork.c log.c sigchld.c spy.c parasite.h clean test
	$(CC) $(CFLAGS) $(CFLAGS_COMPEL) $(shell $(COMPEL) includes) $< vma.c fork.c log.c sigchld.c -o $@ spy.c $(shell $(COMPEL) --static libs)

parasite.h: parasite.po
	$(COMPEL) hgen -o $@ -f $<

parasite.po: parasite.o
	ld $(shell $(COMPEL) ldflags) -o $@ $^ $(shell $(COMPEL) plugins)

parasite.o: parasite.c
	$(CC) $(CFLAGS_COMPEL) -c $(shell $(COMPEL) cflags) -o $@ $^

test:
	make -C test

clean:
	@rm -f libuffd.so
	@rm -f parasite.o
	@rm -f parasite.po
	make -C test clean

.PHONY: all run1 run2 run3 run4 test clean