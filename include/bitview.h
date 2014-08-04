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

#include "bits.h"

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

namespace bitvector
{
    template<template<typename ...> class Container>
    class bitview
    {
    public:
        using value_type = uint64_t;
        using container_type = Container<value_type>;
        
        static constexpr size_t W = bitsize<value_type>();
        
        bitview() = default;
        
        bitview(size_t size)
        : _container(required_container_size(size)) { }
        
        bitview(bitview const&) = default;
        bitview(bitview &&) = default;
        bitview &operator=(bitview const&) = default;
        bitview &operator=(bitview &&) = default;
        
        container_type const&container() const { return _container; }
        container_type      &container()       { return _container; }
        
        // Number of bits handled by the view
        size_t size() const { return _container.size() * W; }
        
        // Change the size of the view, possibly changing the size of the underlying
        // container as well
        void resize(size_t size) {
            return _container.resize(required_container_size(size));
        }
        
        //private:
        static size_t required_container_size(size_t size) {
            return size_t(std::ceil(float(size) / W));
        }
        
        struct range_location_t {
            size_t index;
            size_t lbegin;
            size_t llen;
            size_t hlen;
        };
        
        range_location_t locate(size_t begin, size_t end) const;
        
        std::pair<value_type, bool>
        sum_with_carry(value_type op1, value_type op2, bool carry, size_t width);
        
        template<template<typename ...> class C>
        bool set_sum(bitview<C> other, size_t begin, size_t end, size_t to);
        
        value_type get(size_t begin, size_t end) const;
        bool get(size_t i) const;
        
        void set(size_t begin, size_t end, value_type value);
        void set(size_t i, bool bit);
        
        template<template<typename ...> class C>
        void set(bitview<C> const&other, size_t begin, size_t end, size_t to);
        
    private:
        container_type _container;
    };
    
    /*
     * Class implementation
     */
    template<template<typename ...> class C>
    auto bitview<C>::locate(size_t begin, size_t end) const -> range_location_t
    {
        size_t index  = begin / W;
        size_t lbegin = begin % W;
        
        size_t len = end - begin;
        size_t llen = std::min(W - lbegin, len);
        size_t hlen = len - llen;
        
        return { index, lbegin, llen, hlen };
    }
    
    template<template<typename ...> class Container>
    auto bitview<Container>::get(size_t begin, size_t end) const -> value_type
    {
        if(is_empty_range(begin, end))
            return 0;
        
        assert(end - begin <= W);
        
        range_location_t loc = locate(begin, end);
        
        value_type low = bitfield(_container[loc.index],
                                  loc.lbegin, loc.lbegin + loc.llen);
        
        value_type high = 0;
        if(loc.hlen != 0)
            high = lowbits(_container[loc.index + 1], loc.hlen) << loc.llen;
            
            return high | low;
    }
    
    template<template<typename ...> class Container>
    bool bitview<Container>::get(size_t i) const {
        assert(i < size());
        
        return _container[i / W] & (value_type(1) << (i % W));
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
    template<template<typename ...> class C>
    auto bitview<C>::sum_with_carry(value_type op1, value_type op2,
                                    bool carry, size_t width)
    -> std::pair<value_type, bool>
    {
        assert(width <= W);
        
        value_type a = lowbits(op1, width);
        value_type b = lowbits(op2, width);
        value_type c = carry;
        
        value_type result = lowbits(a + b + c, width);
        
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
            
            value_type a = other.get(src, src + step);
            value_type b = get(dest, dest + step);
            
            value_type result;
            std::tie(result, carry) = sum_with_carry(a, b, carry, step);
            set(dest, dest + step, result);
        }
        
        assert(src == end);
        assert(dest == to + (end - begin));
        assert(len == 0);
        
        return carry;
    }
    
    template<template<typename ...> class Container>
    void bitview<Container>::set(size_t begin, size_t end, value_type value)
    {
        if(is_empty_range(begin, end))
            return;
        
        assert(end - begin <= W);
        
        range_location_t loc = locate(begin, end);
        
        set_bitfield(_container[loc.index], loc.lbegin, loc.lbegin + loc.llen, value);
        
        if(loc.hlen != 0) {
            value_type bits = bitfield(value, loc.llen, loc.llen + loc.hlen);
            set_bitfield(_container[loc.index + 1], 0, loc.hlen, bits);
        }
    }
    
    template<template<typename ...> class Container>
    void bitview<Container>::set(size_t i, bool bit)
    {
        assert(i < size());
        
        size_t word = i / W;
        size_t index = i % W;
        size_t mask = value_type(1) << index;
        
        _container[word] = (_container[word] & ~mask) |
                            (value_type(bit) << index);
    }
    
} // namespace bitvector

#endif
