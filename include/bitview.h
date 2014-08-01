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
    using container_t = Container<T>;
    
    static constexpr size_t W = sizeof(T) * 8;

public:
    bitview() = default;
    
    bitview(size_t size)
        : _container(required_container_size(size)) { }
    
    bitview(bitview const&) = default;
    bitview(bitview &&) = default;
    bitview &operator=(bitview const&) = default;
    bitview &operator=(bitview &&) = default;
    
    container_t const&container() const { return _container; }
    container_t      &container()       { return _container; }
    
    // Number of bits handled by the view
    size_t size() const { return _container.size() * W; }

//private:
    static size_t required_container_size(size_t size) {
        return size_t(std::ceil(float(size) / W));
    }
    
    size_t nbits() const { return _width * size(); }
    
    T mask(size_t begin, size_t end) const;
    T lowbits(T val, size_t n) const;
    T highbits(T val, size_t n) const;
    
    struct range_location_t {
        size_t index;
        size_t header_begin;
        size_t header_len;
        size_t body_size;
        size_t footer_len;
    };
    
    range_location_t locate(size_t begin, size_t end) const;
    
    std::pair<T, bool>
    sum_with_carry(T op1, T op2, bool carry, size_t width);
    
    template<template<typename ...> class C>
    bool set_sum(bitview<C> other, size_t begin, size_t end, size_t to);
    
    void repeat(size_t begin, size_t end, T pattern, size_t pattern_width);
    
    
    T get(size_t begin, size_t end) const;
    bool get(size_t i) const;
    
    void set(size_t begin, size_t end, T value);
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
    
    T low = highbits(_container[loc.index], loc.header_len) >> loc.header_begin;
    T high = 0;
    
    if(loc.footer_len != 0)
        high = lowbits(_container[loc.index + 1], loc.footer_len) << loc.header_len;
    
    return high | low;
}

template<template<typename ...> class Container>
bool bitview<Container>::get(size_t i) const {
    assert(i < size());
    
    return _container[i / W] & (T(1) << (i % W));
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

// Sums the operands (with the eventual previous carry)
// with given precision and returns the result as well as the carry
template<template<typename ...> class Container>
auto bitview<Container>::sum_with_carry(T op1, T op2,
                                        bool carry, size_t width)
                                                           -> std::pair<T, bool>
{
    assert(width <= W);
    
    // FIXME: Check for uint128_t availability and devise another solution
    T a = lowbits(op1, width);
    T b = lowbits(op2, width);
    T c = carry;
    
    T result = lowbits(a + b + c, width);
    
    carry = result < a || result < b || result < c;
    
    return { result, carry };
}

template<template<typename ...> class Container>
template<template<typename ...> class C>
bool bitview<Container>::set_sum(bitview<C> other,
                                 size_t begin, size_t end, size_t to)
{
    if(is_empty_range(begin, end))
        return false;
    
    size_t len   = end - begin;
    size_t rem   = len % W;
    
    size_t src, dest, step;
    bool carry = false;
    
    for(src = begin, dest = to;
        src < end;
        len -= step, src += step, dest += step)
    {
        step = len < W ? rem : W;
        
        T a = other.get(src, src + step);
        T b = get(dest, dest + step);
        
        T result;
        std::tie(result, carry) = sum_with_carry(a, b, carry, step);
        set(dest, dest + step, result);
    }
    
    assert(src == end);
    assert(dest == to + (end - begin));
    assert(len == 0);
    
    return carry;
}

template<template<typename ...> class Container>
void bitview<Container>::set(size_t begin, size_t end, T value)
{
    if(is_empty_range(begin, end))
        return;
    
    assert(end - begin <= W);
    
    range_location_t loc = locate(begin, end);
    size_t index = loc.index;
    size_t llen = loc.header_len;
    size_t hlen = loc.footer_len;
    
    _container[index] = lowbits(_container[index], W - llen) |
                        lowbits(value, llen) << (W - llen);
    
    if(hlen)
        _container[index + 1] = highbits(_container[index + 1], W - hlen) |
                                    ((value & mask(llen, llen + hlen)) >> llen);
}

template<template<typename ...> class Container>
void bitview<Container>::set(size_t i, bool bit)
{
    assert(i < size());
    
    size_t word = i / W;
    size_t index = i % W;
    size_t mask = T(1) << index;
    
    _container[word] = (_container[word] & ~mask) | (T(bit) << index);
}

#endif
