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
    
    constexpr bool is_empty_range(size_t begin, size_t end) {
        return begin >= end;
    }
    
    template<typename T,
             REQUIRES(std::is_integral<T>::value)>
    std::string to_binary(T val, size_t sep = 8, char ssep = ' ')
    {
        std::string s;
        size_t W = sizeof(T) * 8;
        
        for(size_t i = 0; i < W; i++)
        {
            if(i && i % sep == 0)
                s.push_back(ssep);
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
        
        return static_cast<T>(bit) << index                |
               (get_bitfield(word, index, W) << index + 1) |
                get_bitfield(word, 0, index);
    }
    
    template<size_t W>
    class word
    {
        static_assert(W % 64 == 0,
                      "The word width must be a multiple of 64 bits");
        
        static constexpr size_t _size = W / (sizeof(uint64_t) * 8);
        static constexpr uint64_t _ff = std::numeric_limits<uint64_t>::max();
        
    public:
        word() = default;
        word(std::initializer_list<uint64_t> data) {
            assert(data.size() <= _size);
            
            std::copy(data.begin(), data.end(), _data);
        }
        
        word(word const&) = default;
        word &operator=(word const&) = default;
        
        uint64_t const(&data() const)[_size] { return _data; }
        uint64_t      (&data()      )[_size] { return _data; }
        
        size_t size() const { return _size; }
        
        uint64_t get(size_t begin, size_t end) const
        {
            if(is_empty_range(begin, end))
                return 0;
            
            assert(end - begin <= 64);
            
            size_t index, l, llen, hlen;
            std::tie(index, l, llen, hlen) = locate(begin, end);
            
            uint64_t low = highbits(_data[index], llen) >> (64 - llen);
            uint64_t high = 0;
            
            if(hlen != 0)
                high = lowbits(_data[index + 1], hlen) << llen;
            
            return high | low;
        }
        
        bool get(size_t i) const {
            assert(i < W);
            
            return _data[i / 64] & (uint64_t(1) << (i % 64));
        }
        
        void set(size_t begin, size_t end, uint64_t value)
        {
            if(is_empty_range(begin, end))
                return;
            
            assert(end - begin <= 64);
            
            size_t index, l, llen, hlen;
            std::tie(index, l, llen, hlen) = locate(begin, end);
            
            _data[index] = lowbits(_data[index], 64 - llen) |
                           lowbits(value, llen) << (64 - llen);
            
            if(hlen)
                _data[index + 1] = highbits(_data[index + 1], 64 - hlen) |
                                   ((value & mask(llen, llen + hlen)) >> llen);
        }
        
        void set(size_t i, bool bit)
        {
            assert(i < W);
            
            size_t word = i / 64;
            size_t index = i % 64;
            size_t mask = uint64_t(1) << index;
            
            _data[word] = (_data[word] & ~mask) | (uint64_t(bit) << index);
        }
        
        
    private:
        // Mask with bits set in the interval [begin, end)
        uint64_t mask(size_t begin, size_t end) const
        {
            if(is_empty_range(begin, end))
                return 0;
            
            assert(begin < 64);
            assert(end - begin <= 64);
            
            return ((_ff << (64 - end)) >> (64 - end + begin)) << begin;
        }
        
        uint64_t lowbits(uint64_t val, size_t n) const
        {
            return val & mask(0, n);
        }

        uint64_t highbits(uint64_t val, size_t n) const
        {
            return val & mask(64 - n, 64);
        }
        
        std::tuple<size_t, size_t, size_t, size_t>
        locate(size_t begin, size_t end) const {
            size_t index  = begin / 64;
            size_t l      = begin % 64;
            
            size_t len = end - begin;
            size_t llen = std::min(64 - l, len);
            size_t hlen = len - llen;
            
            return { index, l, llen, hlen };
        }
        
    private:
        uint64_t _data[_size] = { };
    };
    
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
        
        array_view() = default;
        
        template<typename Container,
                 typename = decltype(std::declval<T *&>() = std::declval<Container&>().data()),
                 typename = decltype(std::declval<size_t&>() = std::declval<Container&>().size())>
        array_view(Container &container)
            : _data(container.data()), _size(container.size()) { }
        
        array_view(T *data, size_type size)
            : _data(data), _size(size)
        {
            assert(data != nullptr || size == 0);
        }
        
        array_view(array_view const&) = default;
        array_view(array_view &&) = default;
        
        array_view &operator=(array_view const&) = default;
        array_view &operator=(array_view &&) = default;
        
        size_type size() const { return _size; }
        
        bool empty() const { return size() == 0; }
        
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
