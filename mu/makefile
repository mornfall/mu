.CURDIR ?= .
CXXFLAGS += -MD -MP -I$(.CURDIR) -std=c++17 -I/usr/local/include -Wextra -Wall -Werror \
            -fcolor-diagnostics -I$(.CURDIR)/bricks
LDADD += -L/usr/local/lib -licuuc

SRC_common = doc/convert.cpp
SRC = $(SRC_common) slides.cpp paper.cpp pic.cpp main.cpp
BIN = mu mu-pic

LIB = ${SRC_common:%.cpp=%.o}
OBJ = ${SRC:%.cpp=%.o}
DEP = ${SRC:%.cpp=%.d}

.SUFFIXES: .pdf .mu

all: $(BIN)

mu-slides: slides.o $(LIB)
	$(CXX) -o $@ $(LIB) slides.o $(LDADD)

mu-paper: paper.o $(LIB)
	$(CXX) -o $@ $(LIB) paper.o $(LDADD)

mu-pic: pic.o $(LIB)
	$(CXX) -o $@ $(LIB) pic.o $(LDADD)

mu: main.o $(LIB)
	$(CXX) -o $@ $(LIB) main.o $(LDADD)

svgtex: svgtex.cpp
	$(CXX) $(CXXFLAGS) $$(pkg-config --cflags --libs poppler-glib) -o svgtex $(.CURDIR)/svgtex.cpp

clean:
	rm -f $(OBJ) $(DEP) $(BIN) svgtex

.cpp.o:
	@mkdir -p $$(dirname $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.mu.pdf: mu-slides
	@mkdir -p $$(dirname $@)
	./mu-slides $< > $*.tex
	env TEXINPUTS=$(.CURDIR)/dat context --result=$*.pdf $*.tex

-include $(DEP)
