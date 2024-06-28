CC		= gcc
CFLAGS	= -g -O2
SRC		= t01 t03

all: $(SRC)

t01: t01.c
	$(CC) $(CFLAGS) -o $@ $<

t03: t03.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -f $(SRC)