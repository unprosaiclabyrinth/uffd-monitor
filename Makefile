CXX = gcc
CXXFLAGS = -Wall -Wextra -shared -fPIC -g

all: libuffd.so

libuffd.so: uffd.c clean
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f libuffd.so

.PHONY: clean