CXX = gcc
CXXFLAGS = -Wall -Wextra -shared -fPIC

all: libuffd.so

libuffd.so: uffd.c clean test
	$(CXX) $(CXXFLAGS) -o $@ $<

run:
	sudo LD_PRELOAD=./libuffd.so ./test/t01

test:
	make -C test

clean:
	rm -f libuffd.so
	make -C test clean

.PHONY: clean run test