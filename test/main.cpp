/*
 * Copyright 2014 Nicola Gigante
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#include "bitvector.h"
#include "bitview.h"

#include <vector>

#include <iostream>

#include <x86intrin.h>

//using namespace bitvector;

void test_word()
{
    bitview<std::vector> w(1, 256);
    
    assert(w.container().size() == w.required_length(1, 256));
    
    w.container()[0] = std::numeric_limits<uint64_t>::max();
    
    size_t begin = 64 + 56;
    size_t end = 64 + 56 + 16;
    
    w.set(5, false);
    w.set(begin, end, 12345);
    w.set(195, true);
    
    // 0x3039 is 12345
    assert(w.container()[0] == 0xFFFFFFFFFFFFFFDF);
    assert(w.container()[1] == 0x3900000000000000);
    assert(w.container()[2] == 0x30);
    assert(w.container()[3] == 8);
    
    assert(not w.get(5));
    assert(w.get(begin, end) == 12345);
    assert(w.get(195));
    
    bitview<std::vector> w2(1, 256);
    
    w2.set(w, begin, end, 42);

    assert(w2.get(42, 42 + (end - begin)) == 12345);
}

int main()
{
    test_word();
    
    return 0;
}

