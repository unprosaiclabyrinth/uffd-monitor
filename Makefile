CXX = gcc
CXXFLAGS = -Wall -shared -fPIC

libuffd.so: uffd.c
	$(CXX) $(CXXFLAGS) -o $@ $^