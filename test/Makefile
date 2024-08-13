CC		= gcc
CFLAGS	= -g -O2
SRC		= t01 t02 t03 t04

all: $(SRC) t06

t01: t01.c
	$(CC) $(CFLAGS) -o $@ $<

t02: t02.c
	$(CC) $(CFLAGS) -o $@ $<

t03: t03.c
	$(CC) $(CFLAGS) -o $@ $<

t04: t04.c
	$(CC) $(CFLAGS) -o $@ $<

t05:
	make -C t05 build

t06:
	make -C t06

clean:
	@rm -f $(SRC)
	make -C t06 clean

.PHONY: all t01 t02 t03 t04 t06 clean
