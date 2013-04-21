CXXFLAGS ?= -g -I. -std=c++0x -O3
PREFIX ?= /usr/local
EXE ?= jdent

all: $(EXE)

$(EXE): indent.cc json.h
	c++ $(CXXFLAGS) -o $@ indent.cc

install:
	cp $(EXE) $(PREFIX)/bin

clean:
	rm -f $(EXE) *.o core
