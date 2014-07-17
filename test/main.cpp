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

#include "bitvector.h"

#include <vector>
#include <numeric>
#include <chrono>
#include <map>

#include <iostream>

using namespace bitvector;

void test_packed_array()
{
    word_t<64> v[7];
    
    packed_view<64, array_view, flag_bit> pv(10, 20, v, 7);
    
    word_t<64> i = 0;
    for(auto &&x : pv)
    {
        x = i++;
    }
    
    pv(0,4) = 595434449450;
    
    std::cout << to_binary(word_t<64>(595434449450), 10) << "\n";
    std::cout << to_binary(word_t<64>(pv(0, 4)), 10) << "\n";
    
    pv[0] -= 2;
    pv[1] += 2;
    pv[2] >>= 1;
    pv[3] <<= 1;
    
    for(size_t i = 0; i < 4; i++)
        std::cout << pv[i] << "\n";
    
    std::cout << to_binary(word_t<64>(595434449450), 10) << "\n";
    std::cout << to_binary(word_t<64>(pv(0, 4)), 10) << "\n";
}

void test_popcount()
{
    constexpr auto v = (word_t<128>(1) << 64) + 1;
    
    static_assert(popcount(v) == 2, "popcount() doesn't work");
}

void test_insert_bit()
{
    word_t<32> w = 0x7FFFFFFF;
    
    w = insert_bit(w, 15, 0);
    
    assert(w == 0xFFFF7FFF);
}

#define LEAVES_POPCOUNT 0
#define DUMP 0

int main()
{
    bitvector<64> v(100000);
    
    std::cout << v << "\n";
    
    size_t nbits = 99999;
    
    auto t1 = std::chrono::steady_clock::now();
    for(size_t i = 0; i < nbits; ++i)
    {
        v.insert(i, true);
    }
    
    v.insert(nbits, true);
    
    auto t2 = std::chrono::steady_clock::now();
    
    std::cout << float(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count())/1000 << "s\n";
    
#if LEAVES_POPCOUNT
    std::map<size_t, size_t> stats;
    for(auto leaf : v.leaves())
        ++stats[popcount(leaf)];
    
    for(auto p : stats)
        std::cout << "|" << p.first << "|: " << p.second << "\n";
#endif
    
#if DUMP
    std::cout << "Root:\n";
    std::cout << v.root() << "\n";
    
    for(size_t i = 0; i < v.root().nchildren(); ++i)
    {
        std::cout << v.root().child(i) << "\n";
    }
    
    if(v.root().height() == 1) {
        std::cout << "Leaves:\n";
        for(size_t i = 0; i < v.root().nchildren(); i++) {
            assert(v.root().pointers(i) != 0);
            
            std::cout << "[" << i << "] = " << v.root().child(i).size() << "\n";
            std::cout << to_binary(v.root().child(i).leaf()) << "\n";
        }
    }
    
    std::cout << "\nContents:\n";
    for(size_t i = 0; i < nbits; i++)
    {
        if(i && i % 8 == 0)
            std::cout << ' ';
        if(i && i % 40 == 0)
            std::cout << "\n";
        std::cout << v.access(i);
    }
    
    std::cout << "\n";
#endif
    //test_packed_array();
    
    return 0;
}

