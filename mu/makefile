.CURDIR ?= .
CXXFLAGS += -MD -MP -I$(.CURDIR) -std=c++17 -I/usr/local/include
LDADD += -L/usr/local/lib -licuuc

SRC_common = doc/convert.cpp
SRC = $(SRC_common) slides.cpp
BIN = mu-slides

LIB = ${SRC_common:%.cpp=%.o}
OBJ = ${SRC:%.cpp=%.o}
DEP = ${SRC:%.cpp=%.d}

.SUFFIXES: .pdf .mu

all: $(BIN)

mu-slides: slides.o $(LIB)
	$(CXX) -o $@ $(LIB) slides.o $(LDADD)

clean:
	rm -f $(OBJ) $(DEP) $(BIN)

.cpp.o:
	@mkdir -p $$(dirname $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.mu.pdf: mu-slides
	@mkdir -p $$(dirname $@)
	./mu-slides $< > $*.tex
	env TEXINPUTS=$(.CURDIR)/dat context --result=$*.pdf $*.tex

-include $(DEP)
