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
#include <random>

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

void test_word()
{
    word<256> w;
    
    w.data()[0] = std::numeric_limits<uint64_t>::max();
    
    size_t begin = 64 + 56;
    size_t end = 64 + 56 + 16;
    
    w.set(5, false);
    w.set(begin, end, 12345);
    w.set(195, true);
    
    // 0x3039 is 12345
    assert(w.data()[0] == 0xFFFFFFFFFFFFFFDF);
    assert(w.data()[1] == 0x3900000000000000);
    assert(w.data()[2] == 0x30);
    assert(w.data()[3] == 8);
    
    assert(not w.get(5));
    assert(w.get(begin, end) == 12345);
    assert(w.get(195));
}

int main()
{
    test_word();
#if 0
    
#define LEAVES_POPCOUNT 0
#define DUMP 0
    bitvector<64> v(100000);
    
    std::cout << v << "\n";
    
    size_t nbits = 99999;
    
    std::vector<std::pair<int, bool>> data;
    
    std::mt19937 engine(42);
    std::bernoulli_distribution booldist;
    
    for(size_t i = 0; i < nbits; ++i)
    {
        std::uniform_int_distribution<> dist(0, int(i));
        data.push_back({dist(engine), booldist(engine)});
    }
    
    auto t1 = std::chrono::steady_clock::now();
    for(size_t i = 0; i < nbits; ++i)
        v.insert(data[i].first, data[i].second);
    //v.insert(0, true);
    auto t2 = std::chrono::steady_clock::now();
    
#if LEAVES_POPCOUNT
    std::cout << "Leaves minimum size: " << v.leaves_minimum_size() << "\n";
    std::cout << "Used leaves: " << v.used_leaves() << "\n";
    std::cout << "Used nodes: " << v.used_nodes() << "\n";
    std::map<size_t, size_t> stats;
    for(auto leaf : v.leaves())
        ++stats[popcount(leaf)];
    
    for(auto p : stats)
        std::cout << "|" << p.first << "|: " << p.second << "\n";
#endif
    
#if DUMP
//    std::cout << "Root:\n";
//    std::cout << v.root() << "\n";
//    
//    for(size_t i = 0; i < v.root().nchildren(); ++i)
//    {
//        if(!v.root().pointers(i))
//            std::cout << "Child " << i << " should exist but is null\n";
//        else
//            std::cout << v.root().child(i) << "\n";
//    }
    
//    if(v.root().height() == 1) {
//        std::cout << "Leaves:\n";
//        for(size_t i = 0; i < v.root().nchildren(); i++) {
//            assert(v.root().pointers(i) != 0);
//            
//            std::cout << "[" << i << "] = " << v.root().child(i).size() << "\n";
//            std::cout << to_binary(v.root().child(i).leaf()) << "\n";
//        }
//    }
    
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
    
    std::cout << float(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count())/1000 << "s\n";
#endif
    return 0;
}

