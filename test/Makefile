CLANG=clang++
GCC=g++-4.8

INCLUDE_FLAGS=-I../include/
OPTFLAGS=-Ofast -fstrict-aliasing
CLANG_CXXFLAGS=$(INCLUDE_FLAGS) $(OPTFLAGS) -Weverything -pedantic -Wno-c++98-compat -Wno-c++98-compat-pedantic
GCC_CXXFLAGS=$(INCLUDE_FLAGS) $(OPTFLAGS) -Wall -pedantic

INCLUDES=../include/internal/bits.h \
         ../include/internal/bitvector.hpp \
         ../include/bitvector.h \
         ../include/packed_view.h \
         ../include/bitview.h

all: test-clang test-gcc
	@echo "Test of $(CLANG) binary" && ./test-clang && \
     echo "Test with $(CLANG) binary completed"     || \
     echo "Test with $(CLANG) binary failed!!"
	@echo "Test of $(CLANG) binary" && ./test-gcc   && \
     echo "Test with $(GCC) binary completed"       || \
     echo "Test with $(GCC) binary failed!!"

nonexistent:

test-clang: main.cpp $(INCLUDES)
	@echo "Compiling test with $(CLANG)..."
	@$(CLANG) -std=c++11 $(CLANG_CXXFLAGS) -o test-clang main.cpp

test-gcc: main.cpp $(INCLUDES)
	@echo "Compiling test with $(GCC)..."
	@$(GCC) -std=c++11 $(GCC_CXXFLAGS) -o test-gcc main.cpp

clean:
	@rm -f test-clang test-gcc
