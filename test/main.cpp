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
#include "packed_view.h"

#include <vector>
#include <array>

#include <iostream>

//using namespace bitvector;

void test_packed_view()
{
    packed_view<std::vector> v(12, 27);
    
    v(2, 4) = 42;
    
    assert(v[2] == 42);
    assert(v[3] == 42);
    
    v[3] = 1234;
    assert(v[3] == 1234);
    
    v[5] = 1234; // This should lay between two words
    assert(v[5] == 1234);
}

void test_word()
{
    bitview<std::vector> w(256);
    
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
    
    bitview<std::vector> w2(256);
    
    w2.set(w, begin, end, 42);

    assert(w2.get(42, 42 + (end - begin)) == 12345);
    
    uint64_t result;
    bool carry;

    std::tie(result, carry) = w.sum_with_carry(200, 56, false, 8);
    assert(result == 0);
    assert(carry);
    
    std::tie(result, carry) = w.sum_with_carry(200, 56, false, 9);
    assert(result == 256);
    assert(not carry);
    
    std::tie(result, carry) = w.sum_with_carry(254, 1, true, 8);
    assert(result == 0);
    assert(carry);
    
    w2.set_sum(w, begin, end, 42);
    
    assert(w2.get(42, 42 + (end - begin)) == 24690);
}

void test_bits()
{
    uint64_t val = 0x0000000000000000;

    set_bitfield(val, 16, 16 + 8, uint64_t(42));
    
    assert(bitfield(val, 16, 16 + 8) == 42);
    
    bitview<std::vector> w(64);
    
    w.set(16, 16 + 8, 42);
    
    assert(w.container()[0] == val);
    assert(w.get(16, 16 + 8) == 42);
}

int main()
{
    test_bits();
    test_word();
    test_packed_view();
    
    return 0;
}

