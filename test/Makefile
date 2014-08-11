#
# Copyright 2014 Nicola Gigante
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


INCLUDE_FLAGS=-I../include/

CLANG_CXXFLAGS=-Weverything -pedantic -Wno-c++98-compat -Wno-c++98-compat-pedantic
GCC_CXXFLAGS=-Wall -pedantic

ifndef STD
	STD=c++11
endif

ifndef OPTFLAGS
	ifneq (yes,$(DEBUG))
		ifdef ARCH
			ARCHFLAGS=-march=$(ARCH) -mtune=generic
		else
			ARCHFLAGS=-mtune=generic
		endif
		ifneq (yes, $(ASSERTS))
			ASSERTSFLAGS=-DNDEBUG
		else
			ASSERTSFLAGS=
		endif
		OPTFLAGS=-Ofast -fstrict-aliasing $(ASSERTSFLAGS) $(ARCHFLAGS)
	else
		OPTFLAGS=-O0 -ggdb
	endif
endif


CCVER=$(shell $(CXX) --version)

ifneq (,$(findstring g++,$(CXX)))
	MY_CXXFLAGS=$(GCC_CXXFLAGS) $(CXXFLAGS)
else
	MY_CXXFLAGS=$(CLANG_CXXFLAGS) $(CXXFLAGS)
endif

ifeq (c++,$(CXX))
	TARGET=test
else
	TARGET=test-$(CXX)
endif

INCLUDES=../include/internal/bits.h \
         ../include/internal/bitvector.hpp \
         ../include/bitvector.h \
         ../include/packed_view.h \
         ../include/bitview.h

all: $(TARGET)

nonexistent:

$(TARGET): main.cpp $(INCLUDES)
	@echo "Compiling test with $(CXX)..."
	@$(CXX) -std=$(STD) $(MY_CXXFLAGS) $(INCLUDE_FLAGS) $(OPTFLAGS) -o $(TARGET) main.cpp

clean:
	@rm -f test test-*
