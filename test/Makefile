CC		= gcc
CFLAGS	= -g -O2
SRC		= t01 t02 t03 t04

all: $(SRC)

t01: t01.c
	$(CC) $(CFLAGS) -o $@ $<

t02: t02.c
	$(CC) $(CFLAGS) -o $@ $<

t03: t03.c
	$(CC) $(CFLAGS) -o $@ $<

t04: t04.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -f $(SRC)