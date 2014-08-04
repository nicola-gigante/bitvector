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
#include <string>
#include <type_traits>

// FIXME: remove this header when finished the debugging
#include <iostream>

#define REQUIRES(...) \
typename = typename std::enable_if<(__VA_ARGS__)>::type

namespace bitvector
{
    // Utility function to check for an empty range,
    // used through all the code
    constexpr bool is_empty_range(size_t begin, size_t end) {
        return begin >= end;
    }
    
    // Returns the bit size of the given type
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    constexpr size_t bitsize() {
        return sizeof(T) * CHAR_BIT;
    }
    
    /*
     * Count the number of set bits in a word
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    constexpr size_t popcount(T value)
    {
        static_assert(std::is_same<T, uint64_t>::value,
                      "Popcount is unimplemented for types other than uint64_t");
        // TODO: Check the availability of __builtin_popcountll
        return __builtin_popcountll(value);
    }
    
    /*
     * Bitmasking functions
     */
    
    /*
     * Returns a mask word for ask with bits set in the interval [begin, end)
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    T mask(size_t begin, size_t end)
    {
        constexpr size_t W = bitsize<T>();
        
        if(is_empty_range(begin, end))
            return 0;
        
        assert(begin < W);
        assert(end - begin <= W);
        
        constexpr T ff = std::numeric_limits<T>::max();
        
        return ((ff << (W - end)) >> (W - end + begin)) << begin;
    }
    
    /*
     * Returns the lower n bits in the word
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    T lowbits(T val, size_t n)
    {
        return val & mask<T>(0, n);
    }
    
    /*
     * Returns the higher n bits in the word.
     * Note that bits are not shifted, simply lower bits get masked away.
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    T highbits(T val, size_t n)
    {
        constexpr size_t W = bitsize<T>();
        
        return val & mask<T>(W - n, W);
    }
    
    /*
     * Returns the _value_ contained in the bits in the interval [begin, end)
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    T bitfield(T val, size_t begin, size_t end)
    {
        constexpr size_t W = bitsize<T>();
        
        if(is_empty_range(begin, end))
            return 0;
        
        assert(begin < W);
        assert(end - begin <= W);
        
        return lowbits(highbits(val, W - begin) >> begin, end - begin);
    }
    
    /*
     * Set the value of the bits in the interval [begin, end) to the
     * corresponding low bits of 'value'
     */
    template<typename T>
    void set_bitfield(T &dest, size_t begin, size_t end, T value)
    {
        if(is_empty_range(begin, end))
            return;
        
        size_t len = end - begin;
        
        T masked = lowbits(value, len) << begin;
        T zeroes = ~mask<T>(begin, end);
        
        dest = (dest & zeroes) | masked;
    }
    
    /*
     * Returns the specified bit in the word
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    bool bit(T word, size_t index)
    {
        return bool(bitfield(word, index, index + 1));
    }
    
    /*
     * Inserts the specified bit inside the word at the specified position,
     * shifting left all the more significant bits. The MSB is lost.
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    T insert_bit(T word, size_t index, bool bit)
    {
        const size_t W = sizeof(T) * 8;
        
        return static_cast<T>(bit) << index            |
               (bitfield(word, index, W) << index + 1) |
                bitfield(word, 0, index);
    }
    
    /*
     * Render the word as a binary string.
     * Every 'sep' bits, the char ssep is printed.
     */
    template<typename T, REQUIRES(std::is_integral<T>::value)>
    std::string to_binary(T val, size_t sep = 8, char ssep = ' ')
    {
        std::string s;
        constexpr size_t W = sizeof(T) * 8;
        
        for(size_t i = 0; i < W; i++)
        {
            if(i && i % sep == 0)
                s += ssep;
            s += val % 2 ? '1' : '0';
            val = val / 2;
        }
        
        std::reverse(s.begin(), s.end());
        return s;
    }
} // namespace bitvector
#endif
