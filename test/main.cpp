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

#include "bitview.h"
#include "packed_view.h"
#include "bitvector.h"

#include <vector>
#include <array>

#include <iostream>

using namespace bv;

void test_bits();
void test_word();
void test_packed_view();
void test_bitvector();

void test_bitvector()
{
    constexpr size_t W = 2048;
    constexpr allocation_policy_t AP = alloc_on_demand;
    size_t N = 1000000;
    size_t Wn = 256;
    
    bitvector_t<W, AP>::test(std::cout, N, Wn,
                             /*random      =*/true,
                             /*testrank    =*/false,
                             /*dumpinfo    =*/false,
                             /*dumpnode    =*/false,
                             /*dumpcontents=*/false);
}

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
    
    v[0] = 10;
    v[1] = 20;
    v[2] = 30;
    v[3] = 40;
    v[4] = 50;
    v[5] = 60;
    
    v[4] = v[5];
    
    v(0, 3) += 10;
    assert(v[0] == 20);
    assert(v[1] == 30);
    assert(v[2] == 40);
    assert(v[3] == 40);
    assert(v[4] == 60);
    
    v(0, 3) -= 10;
    assert(v[0] == 10);
    assert(v[1] == 20);
    assert(v[2] == 30);
    assert(v[3] == 40);
    
    assert(to_binary(v(0, 3), 12) == "000000011110 000000010100 000000001010");
    
    // Test the constness and refs conversions
    packed_view<std::vector>::range_reference r = v(3,6);
    packed_view<std::vector>::const_range_reference cr = v(0, 3);
    static_assert(not std::is_convertible<decltype(cr), decltype(r)>::value,
                  "Something wrong with reference constness");
    static_assert(std::is_convertible<decltype(r), decltype(cr)>::value,
                  "Something wrong with reference constness");
    
    // Test range assignment
    r = cr;
    assert(v[3] == 10);
    assert(v[4] == 20);
    assert(v[5] == 30);
    
    // Test std::sort to test the behavior of iterators
    v[0] = 40;
    v[1] = 30;
    v[2] = 20;
    v[3] = 10;
    
    std::sort(v.begin(), v.end());
    
    assert(std::is_sorted(v.begin(), v.end()));
}

void test_word()
{
    bitview<std::vector> w(256);
    
    w.container()[0] = std::numeric_limits<uint64_t>::max();
    
    size_t begin = 64 + 56;
    size_t end = 64 + 56 + 16;
    
    w.set(begin, end, 12345);
    
    w.set(208, 208 + 16, 42);
    
    // 0x3039 is 12345
    assert(w.container()[0] == 0xFFFFFFFFFFFFFFFF);
    assert(w.container()[1] == 0x3900000000000000);
    assert(w.container()[2] == 0x30);
    assert(w.container()[3] == 0x00000000002a0000);
    
    assert(w.get(begin, end) == 12345);
    
    bitview<std::vector> w2(256);
    
    w2.copy(w, begin, end, 42, 42 + 16);

    assert(w2.get(42, 42 + (end - begin)) == 12345);
    
    // Test self backwards copy with overlap
    w.set(50, 60, 42);
    w.set(20, 40, 0xBABE);
    w.copy(w, 20, 50, 30, 50);

    assert(w.get(30, 50) == 0xBABE);
    assert(w.get(50, 60) == 42);
    
    
    w.set(60, 70, 42);
    w.insert(60, false);
    
    assert(w.get(60, 70) == 84);
    
    w.insert(60, true);
    
    assert(w.get(60, 70) == 169);
    assert(w.popcount(60, 70) == 4);
}

template<typename T>
using array4 = std::array<T, 4>;

void test_bits()
{
    uint64_t val = 0x0000000000000000;

    bv::internal::set_bitfield(val, 16, 16 + 8, uint64_t(42));
    
    assert(bv::internal::bitfield(val, 16, 16 + 8) == 42);
    
    bitview<std::vector> w(64);
    
    w.set(16, 16 + 8, 42);
    
    assert(w.container()[0] == val);
    assert(w.get(16, 16 + 8) == 42);
    
    w.set(42, true);
    assert(w.get(42));
    w.set(42, false);
    assert(not w.get(42));
    
    // Test bitview of a static array
    bitarray<256> w2;
    assert(w2.size() == 256);
    w2.set(0, 42, 42);
    assert(w2.get(0, 42) == 42);
}

int main()
{    
    //test_bits();
    //test_word();
    test_packed_view();
    //test_bitvector();
    
    return 0;
}

