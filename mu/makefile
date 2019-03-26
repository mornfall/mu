.CURDIR ?= .
CXXFLAGS += -MD -MP -I$(.CURDIR) -std=c++17 -I/usr/local/include
LDADD += -L/usr/local/lib -licuuc

SRC_common = doc/convert.cpp
SRC = $(SRC_common) slides.cpp
BIN = umd-slides

LIB = ${SRC_common:%.cpp=%.o}
OBJ = ${SRC:%.cpp=%.o}
DEP = ${SRC:%.cpp=%.d}

all: $(BIN)

umd-slides: $(LIB) slides.o
	$(CXX) -o umd-slides $(LIB) slides.o $(LDADD)

clean:
	rm -f $(OBJ) $(DEP) $(BIN)

.cpp.o:
	@mkdir -p $$(dirname $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.umd.pdf: umd-slides
	./umd-slides $< > $@.tex

-include $(DEP)
