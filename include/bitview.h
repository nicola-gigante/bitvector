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

#ifndef BITVECTOR_BITVIEW_H
#define BITVECTOR_BITVIEW_H

#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <limits>
#include <tuple>
#include <algorithm>

// FIXME: remove this header when finished debugging
#include <iostream>

template<template<typename ...> class Container>
class bitview
{
    using T = uint64_t;
    static constexpr size_t W = sizeof(T) * 8;
    using container_t = Container<uint64_t>;
    
public:
    bitview() = default;
    
    bitview(size_t width, size_t size)
        : _container(required_length(width, size)), _width(width) { }
    
    bitview(bitview const&) = default;
    bitview(bitview &&) = default;
    bitview &operator=(bitview const&) = default;
    bitview &operator=(bitview &&) = default;
    
    container_t const&container() const { return _container; }
    container_t      &container()       { return _container; }
    
    // Width of fields presented by the view
    size_t width() const { return _width; }
    
    // Number of fields presented by the view
    size_t size() const { return (_container.size() * W) / _width; }

//private:
    // Suggests the length of a buffer required to store the specified
    // number of elements of the specified width
    size_t required_length(size_t width, size_t size) {
        return size_t(std::ceil((float(width) * size) / W));
    }
    
    size_t nbits() const { return _width * size(); }
    
    uint64_t mask(size_t begin, size_t end) const;
    uint64_t lowbits(uint64_t val, size_t n) const;
    uint64_t highbits(uint64_t val, size_t n) const;
    
    struct range_location_t {
        size_t index;
        size_t header_begin;
        size_t header_len;
        size_t body_size;
        size_t footer_len;
    };
    
    range_location_t locate(size_t begin, size_t end) const;
    
    uint64_t get(size_t begin, size_t end) const;
    bool get(size_t i) const;
    
    void set(size_t begin, size_t end, uint64_t value);
    void set(size_t i, bool bit);
    
    template<template<typename ...> class C>
    void set(bitview<C> const&other, size_t begin, size_t end, size_t to);
    
private:
    container_t _container;
    size_t _width = 1;
};

/*
 * Class implementation
 */
// FIXME: move to some general utility header
constexpr bool is_empty_range(size_t begin, size_t end) {
    return begin >= end;
}

// Mask with bits set in the interval [begin, end)
template<template<typename ...> class Container>
auto bitview<Container>::mask(size_t begin, size_t end) const -> T
{
    if(is_empty_range(begin, end))
        return 0;
    
    assert(begin < W);
    assert(end - begin <= W);
    
    constexpr T ff = std::numeric_limits<T>::max();
    
    return ((ff << (W - end)) >> (W - end + begin)) << begin;
}

template<template<typename ...> class Container>
auto bitview<Container>::lowbits(T val, size_t n) const -> T
{
    return val & mask(0, n);
}

template<template<typename ...> class Container>
auto bitview<Container>::highbits(T val, size_t n) const -> T
{
    return val & mask(W - n, W);
}

//std::tuple<size_t, size_t, size_t, size_t>
//inline locate(size_t begin, size_t end) {
//    size_t index  = begin / 64;
//    size_t l      = begin % 64;
//    
//    size_t len = end - begin;
//    size_t llen = std::min(64 - l, len);
//    size_t hlen = len - llen;
//    
//    return { index, l, llen, hlen };
//}

template<template<typename ...> class Container>
auto bitview<Container>::locate(size_t begin, size_t end) const -> range_location_t
{
    size_t index = begin / W;
    size_t header_begin = begin % W;
    size_t header_len = (W - header_begin) % W;
    size_t body_size = 0;
    size_t footer_len = 0;
 
    size_t len = end - begin;
    if(len > header_len) {
        len -= header_len;
        body_size = len / W;
        footer_len = len % W;
    }
    
    return { index, header_begin, header_len, body_size, footer_len };
}

template<template<typename ...> class Container>
auto bitview<Container>::get(size_t begin, size_t end) const -> T
{
    if(is_empty_range(begin, end))
        return 0;
    
    assert(end - begin <= W);
    
    range_location_t loc = locate(begin, end);
    
    uint64_t low = highbits(_container[loc.index], loc.header_len) >> loc.header_begin;
    uint64_t high = 0;
    
    if(loc.footer_len != 0)
        high = lowbits(_container[loc.index + 1], loc.footer_len) << loc.header_len;
    
    return high | low;
}

template<template<typename ...> class Container>
bool bitview<Container>::get(size_t i) const {
    assert(i < size());
    
    return _container[i / 64] & (uint64_t(1) << (i % 64));
}

template<template<typename ...> class Container>
template<template<typename ...> class C>
void bitview<Container>::set(bitview<C> const&other,
                             size_t begin, size_t end, size_t to)
{
    if(is_empty_range(begin, end))
        return;
    
    size_t len = end - begin;
    
    assert(to < size() - len);
    
    // Pointer to current copy positions
    size_t src = begin;
    size_t dest = to;
    
    size_t n = len / W;
    size_t rem = len % W;
    
    for(size_t i = 0; i < n; ++i, src += W, dest += W)
        set(dest, dest + W, other.get(src, src + W));
        
    set(dest, dest + rem, other.get(src, src + rem));
    
    dest += rem;
    assert(dest - to == len);
}

template<template<typename ...> class Container>
void bitview<Container>::set(size_t begin, size_t end, uint64_t value)
{
    if(is_empty_range(begin, end))
        return;
    
    assert(end - begin <= 64);
    
    range_location_t loc = locate(begin, end);
    size_t index = loc.index;
    size_t llen = loc.header_len;
    size_t hlen = loc.footer_len;
    
    _container[index] = lowbits(_container[index], 64 - llen) |
                        lowbits(value, llen) << (64 - llen);
    
    if(hlen)
        _container[index + 1] = highbits(_container[index + 1], 64 - hlen) |
                                    ((value & mask(llen, llen + hlen)) >> llen);
}

template<template<typename ...> class Container>
void bitview<Container>::set(size_t i, bool bit)
{
    assert(i < size());
    
    size_t word = i / 64;
    size_t index = i % 64;
    size_t mask = uint64_t(1) << index;
    
    _container[word] = (_container[word] & ~mask) | (uint64_t(bit) << index);
}

#endif
