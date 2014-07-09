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

#define REQUIRES(...) \
typename = typename std::enable_if<(__VA_ARGS__)>::type

namespace bitvector
{
    namespace details
    {
        template<size_t W>
        struct word_t { };
        
        
        // FIXME:
        template<>
        struct word_t<8> {
            using type = uint8_t;
            
            static constexpr size_t popcount(type v) {
                return __builtin_popcount(v);
            }
        };
        
        template<>
        struct word_t<16> {
            using type = uint16_t;
            
            static constexpr size_t popcount(type v) {
                return __builtin_popcount(v);
            }
        };
        
        template<>
        struct word_t<32> {
            using type = uint32_t;
            
            static constexpr size_t popcount(type v) {
                return __builtin_popcount(v);
            }
        };
        
        template<>
        struct word_t<64> {
            using type = uint64_t;
            
            static constexpr size_t popcount(type v) {
                return __builtin_popcountll(v);
            }
        };
        
        // FIXME: test __uint128_t availability
        template<>
        struct word_t<128> {
            using type = __uint128_t;

        public:
            static constexpr size_t popcount(type v) {
                
                return word_t<64>::popcount(v >> 64) +
                word_t<64>::popcount((v & (type(0xFFFFFFFFFFFFFFFF) << 64)) >> 64);
            }
        };
    }
    
    /*
     * Generic configurable-size word type. The size parameter is in bits.
     */
    template<size_t W>
    using word_t = typename details::word_t<W>::type;
    
    template<typename T>
    constexpr size_t popcount(T value)
    {
        return details::word_t<sizeof(T) * 8>::popcount(value);
    }
    
    template<typename T>
    std::string to_binary(T val, size_t sep = 8)
    {
        std::string s;
        size_t W = sizeof(T) * 8;
        
        for(size_t i = 0; i < W; i++)
        {
            if(i && i % sep == 0)
                s.push_back(' ');
            s.push_back(val % 2 ? '1' : '0');
            val = val / 2;
        }
        
        std::reverse(s.begin(), s.end());
        return s;
    }
    
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
        
        return (std::numeric_limits<word_t<W>>::max() >> shift) << begin;
    }
    
    template<size_t W>
    word_t<W> zeroes(size_t begin, size_t end)
    {
        return ones<W>(0, begin) | ones<W>(end, W);
    }
    
    // Extracts from the word the bits in the range [begin, end),
    // counting from zero from the LSB.
    template<typename T,
             REQUIRES(std::is_integral<T>::value)>
    T get_bitfield(T word, size_t begin, size_t end)
    {
        const size_t W = sizeof(T) * 8;
        
        assert(end >= begin);
        assert(begin < W);
        assert(end <= W);
        
        const T mask = ones<W>(begin, end);
        
        return (word & mask) >> begin;
    }
    
    template<typename T,
             REQUIRES(std::is_integral<T>::value)>
    bool get_bit(T word, size_t index)
    {
        return bool(get_bitfield(word, index, index + 1));
    }
    
    template<typename T, typename U,
             REQUIRES(std::is_integral<T>::value),
             REQUIRES(std::is_integral<U>::value)>
    T set_bitfield(T word, size_t begin, size_t end, U value)
    {
        const size_t W = sizeof(T) * 8;
        
        assert(end >= begin);
        assert(begin < W);
        assert(end <= W);
        
        word_t<W> mask = zeroes<W>(begin, end);
        
        value = (value << begin) & ~mask;
        
        return (word & mask) | value;
    }
    
    template<typename T,
             REQUIRES(std::is_integral<T>::value)>
    T set_bit(T word, size_t index, bool bit)
    {
        return set_bitfield(word, index, index + 1, bit);
    }
    
    template<typename T,
             REQUIRES(std::is_integral<T>::value)>
    T insert_bit(T word, size_t index, bool bit)
    {
        const size_t W = sizeof(T) * 8;
        
        return static_cast<T>(bit) << index                 |
               (get_bitfield(word, index, W) << index + 1) |
                get_bitfield(word, 0, index);
    }
    
    /*
     * array_view-like class.
     */
    template<typename T>
    class array_view
    {
    public:
        using value_type             = T;
        using reference              = T       &;
        using const_reference        = T const &;
        using pointer                = T       *;
        using const_pointer          = T const *;
        using iterator               = T       *;
        using const_iterator         = T const *;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using size_type              = size_t;
        using difference_type        = ptrdiff_t;
        
        array_view(T *data, size_type size)
        : _data(data), _size(size)
        {
            assert(data != nullptr);
            assert(size != 0);
        }
        
        array_view(array_view const&) = default;
        array_view(array_view &&) = default;
        
        array_view &operator=(array_view const&) = default;
        array_view &operator=(array_view &&) = default;
        
        size_type size() const { return _size; }
        
        const_pointer data() const { return _data; }
        pointer       data()       { return _data; }
        
        iterator       begin()       { return _data; }
        iterator       end()         { return _data + _size; }
        const_iterator begin() const { return _data; }
        const_iterator end()   const { return _data + _size; }
        
        reverse_iterator       rbegin()       { return reverse_iterator(end()); }
        reverse_iterator       rend()         { return reverse_iterator(begin()); }
        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator rend()   const { return const_reverse_iterator(begin()); }
        
        reference operator[](size_type i) {
            assert(i < _size);
            return _data[i];
        }
        
        value_type operator[](size_type i) const {
            assert(i < _size);
            return _data[i];
        }
        
    private:
        T *_data = nullptr;
        size_type _size = 0;
    };
    
} // namespace bitvector
#endif
