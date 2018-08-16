CXXFLAGS ?= -g -I. -std=c++17 -O0
PREFIX ?= /usr/local
EXE ?= jdent rational

all: $(EXE)
rational: json.h rational.cc rational.h


jdent: indent.cc json.h
	c++ $(CXXFLAGS) -o $@ indent.cc

install:
	cp jdent $(PREFIX)/bin

clean:
	rm -f $(EXE) *.o core
