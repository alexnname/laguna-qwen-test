CXX = g++
CXXFLAGS = -std=c++17 -fPIC -O2

all: plugin.so

plugin.so: plugin.cpp
	$(CXX) $(CXXFLAGS) -shared -o $@ $<

clean:
	rm -f plugin.so

.PHONY: all clean
