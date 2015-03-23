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

#ifndef PACKED_BITVECTOR_BITS_H
#define PACKED_BITVECTOR_BITS_H

#include <cstddef>
#include <climits>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>
#include <memory>
#include <algorithm>
#include <iterator>
#include <string>
#include <type_traits>

#define REQUIRES(...) \
typename = typename std::enable_if<(__VA_ARGS__)>::type

namespace bv
{
    namespace internal {
        // Utility trait alias for std::conditional.
        // Would be standard if we used C++14
        template<bool C, typename T, typename F>
        using conditional_t = typename std::conditional<C, T, F>::type;
        
        // Utility trait type to conditionally add constness to a type
        template<bool C, typename T>
        struct add_const_if {
            using type = conditional_t<C, T const, T>;
        };
        
        template<bool C, typename T>
        using add_const_if_t = typename add_const_if<C, T>::type;
        
        template<typename ...Args>
        void unused(Args&& ...) { }
        
        // Utility function to check for an empty range,
        // used through all the code
        constexpr bool is_empty_range(size_t begin, size_t end) {
            return begin >= end;
        }
        
        // Utility function to assert a valid range
        // Note that an empty range is always valid regardless of the value of
        // the bounds used to represent it
        inline bool check_valid_range(size_t begin, size_t end, size_t bound)
        {
            assert(is_empty_range(begin, end) ||
                   (begin < bound && end <= bound));
            unused(begin, end, bound);
            return true;
        }
        
        // Returns ceil(a / b) without casting to floating point
        constexpr size_t ceildiv(size_t a, size_t b) {
            return a / b + (a % b != 0);
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
                          "Popcount is implemented only for uint64_t");
            // TODO: Check the availability of __builtin_popcountll
            return size_t(__builtin_popcountll(value));
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
            
            check_valid_range(begin, end, W);
            
            constexpr T ff = std::numeric_limits<T>::max();
            
            return ((ff << (W - end)) >> (W - end + begin)) << begin;
        }
        

        /*
         * Returns the lower n bits in the word
         */
        template<typename T, REQUIRES(std::is_integral<T>::value)>
        T lowbits(T val, size_t n)
        {
            constexpr size_t W = bitsize<T>();
            assert(n <= W);
            
            size_t shift = (W - n) % W;

            return ((val << shift) >> shift) * (n != 0);
        }
        
        /*
         * Returns the higher n bits in the word.
         * Note that bits are not shifted, simply lower bits get masked away.
         */
        template<typename T, REQUIRES(std::is_integral<T>::value)>
        T highbits(T val, size_t n)
        {
            constexpr size_t W = bitsize<T>();
            assert(n <= W);
            
            size_t shift = (W - n) % W;

            return ((val >> shift) << shift) * (n != 0);
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
            
            check_valid_range(begin, end, W);
            
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
            
            check_valid_range(begin, end, bitsize<T>());
            
            T masked = lowbits(value, end - begin) << begin;
            T zeroes = ~mask<T>(begin, end);
            
            dest = (dest & zeroes) | masked;
        }
        
        // Utility function to assert that a word fits in the given bit width.
        // In other words, the value is less than 2^w - 1
        template<typename T, REQUIRES(std::is_integral<T>::value)>
        bool ensure_bitsize(T value, size_t width)
        {
            assert(lowbits(value, width) == value);
            unused(value, width);
            return true;
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
        
    } // namespace internal
} // namespace bv
#endif
