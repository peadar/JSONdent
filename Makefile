CXXFLAGS = -g -I. -std=c++0x -O3
all: indent


indent: indent.cc json.h
	c++ $(CXXFLAGS) -o $@ indent.cc

