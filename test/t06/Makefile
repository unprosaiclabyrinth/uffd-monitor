CC		= gcc
LD		= ld
CFLAGS	= -g -O0 -ffunction-sections
LDFLAGS = -T linker_script.ld -dynamic-linker /lib64/ld-linux-x86-64.so.2
#LDFLAGS = -T linker_script.ld -L/usr/lib/x86_64-linux-gnu -lc -dynamic-linker /lib64/ld-linux-x86-64.so.2

t06: t06.o
	$(CC) $(LDFLAGS) -o $@ $<
#	$(LD) $(LDFLAGS) -o $@ $< /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o /usr/lib/x86_64-linux-gnu/crtn.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o t06