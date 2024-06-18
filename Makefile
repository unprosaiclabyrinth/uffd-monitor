CXX = gcc
CXXFLAGS = -Wall -shared -fPIC -g

libuffd.so: uffd.c
	$(CXX) $(CXXFLAGS) -o $@ $^