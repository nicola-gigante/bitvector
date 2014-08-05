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
        
        // Change the size of the view, possibly changing the size of the
        // underlying container as well
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
        
        value_type get(size_t begin, size_t end) const;
        
        void set(size_t begin, size_t end, value_type value);
        
        template<template<typename ...> class C>
        void copy_forward(bitview<C> const&src,
                          size_t src_begin, size_t src_end, size_t dest_begin);
        
        template<template<typename ...> class C>
        void copy_backward(bitview<C> const&src,
                           size_t src_begin, size_t src_end, size_t dest_begin);
        
        template<template<typename ...> class C>
        void copy(bitview<C> const&src,
                  size_t src_begin, size_t src_end,
                  size_t dest_begin, size_t dest_end);
        
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
        
        check_valid_range(begin, end, size());
        
        range_location_t loc = locate(begin, end);
        
        value_type low = bitfield(_container[loc.index],
                                  loc.lbegin, loc.lbegin + loc.llen);
        
        value_type high = 0;
        if(loc.hlen != 0)
            high = lowbits(_container[loc.index + 1], loc.hlen) << loc.llen;
            
            return high | low;
    }
    
    template<template<typename ...> class Container>
    template<template<typename ...> class C>
    void bitview<Container>::copy_forward(bitview<C> const&srcbv,
                                          size_t src_begin, size_t src_end,
                                          size_t dest_begin)
    {
        size_t len = (src_end - src_begin);
        size_t rem = len % W;
        
        for(size_t step, src = src_begin, dest = dest_begin;
            src < src_end;
            len -= step, src += step, dest += step)
        {
            step = len < W ? rem : W;
            
            set(dest, dest + step, srcbv.get(src, src + step));
        }
    }
    
    template<template<typename ...> class Container>
    template<template<typename ...> class C>
    void bitview<Container>::copy_backward(bitview<C> const&srcbv,
                                           size_t src_begin, size_t src_end,
                                           size_t dest_begin)
    {
        size_t len = (src_end - src_begin);
        size_t rem = len % W;
        
        for(size_t step, src = src_begin + len, dest = dest_begin + len;
            src - src_begin;
            src -= step, dest -= step)
        {
            step = (src - src_begin) == rem ? rem : W;
            
            set(dest - step, dest, srcbv.get(src - step, src));
        }
    }
    
    template<template<typename ...> class Container>
    template<template<typename ...> class C>
    void bitview<Container>::copy(bitview<C> const&src,
                                  size_t src_begin, size_t src_end,
                                  size_t dest_begin, size_t dest_end)
    {
        size_t srclen = src_end - src_begin;
        size_t destlen = dest_end - dest_begin;
        
        if(destlen < srclen)
            src_end = src_begin + destlen;
        
        if(this == &src && src_begin < dest_begin)
            copy_backward(src, src_begin, src_end, dest_begin);
        else
            copy_forward(src, src_begin, src_end, dest_begin);
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
    
} // namespace bitvector

#endif
