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

#ifndef BITVECTOR_BITS_H
#define BITVECTOR_BITS_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <memory>
#include <cassert>
#include <cmath>
#include <iterator>

#include <iostream>
#include <iomanip>

namespace bitvector
{
    namespace details
    {
        template<size_t W>
        struct word_t { };
        
        template<>
        struct word_t<8> {
            using type = uint8_t;
        };
        
        template<>
        struct word_t<16> {
            using type = uint16_t;
        };
        
        template<>
        struct word_t<32> {
            using type = uint32_t;
        };
        
        template<>
        struct word_t<64> {
            using type = uint64_t;
        };
        
        // FIXME: test __uint128_t availability
        template<>
        struct word_t<128> {
            using type = __uint128_t;
        };
    }
    
    /*
     * Generic configurable-size word type. The size parameter is in bits.
     */
    template<size_t W>
    using word_t = typename details::word_t<W>::type;
    
    // Mask functions
    template<size_t W>
    word_t<W> ones(size_t begin, size_t end)
    {
        if(begin == end)
            return 0;
        
        assert(begin < end);
        assert(begin < W);
        assert(end <= W);
        
        size_t shift = W - end + begin;
        
        return std::numeric_limits<word_t<W>>::max() >> shift << begin;
    }
    
    template<size_t W>
    word_t<W> zeroes(size_t begin, size_t end)
    {
        return ones<W>(0, begin) | ones<W>(end, W);
    }
    
    // Extracts from the word the bits in the range [begin, end),
    // counting from zero from the LSB.
    template<typename T>
    T get_bitfield(T word, size_t begin, size_t end)
    {
        const size_t W = sizeof(T) * 8;
        
        assert(end >= begin);
        assert(begin < W);
        assert(end <= W);
        
        const T mask = ones<W>(begin, end);
        
        return (word & mask) >> begin;
    }
    
    template<typename T, typename U>
    void set_bitfield(T *word, size_t begin, size_t end, U value)
    {
        const size_t W = sizeof(T) * 8;
        
        assert(end >= begin);
        assert(begin < W);
        assert(end <= W);
        
        word_t<W> mask = zeroes<W>(begin, end);
        
        value = (value << begin) & ~mask;
        
        *word = (*word & mask) | value;
    }
    
} // namespace bitvector
#endif
