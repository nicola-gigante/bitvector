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

#ifndef BITVECTOR_PACKED_VIEW_H
#define BITVECTOR_PACKED_VIEW_H

#include "bitview.h"

template<template<typename ...> class Container>
class packed_view
{
    using T           = typename bitview<Container>::T;
    using container_t = typename bitview<Container>::container_t;
    static constexpr size_t W =  bitview<Container>::W;
    
    template<bool>
    class range_reference_base;
    
public:
    using value_type = T;

    class range_reference;
    using const_range_reference = range_reference_base<true>;
    
    packed_view() = default;
    
    packed_view(size_t width, size_t size)
        : _bits(width * size), _width(width) { }
    
    packed_view(packed_view const&) = default;
    packed_view(packed_view &&) = default;
    packed_view &operator=(packed_view const&) = default;
    packed_view &operator=(packed_view &&) = default;
    
    container_t const&container() const { return _bits.container(); }
    container_t      &container()       { return _bits.container(); }
    
    range_reference operator()(size_t begin, size_t end) {
        return { *this, begin, end };
    }
    
    const_range_reference operator()(size_t begin, size_t end) const {
        return { *this, begin, end };
    }
    
    // The number of fields contained in the packed_view
    size_t size() const { return _bits.size() / _width; }
    
private:
    bitview<Container> _bits;
    size_t _width; // Number of bits per field
};

// Const reference and base class for non-const references
template<template<typename ...> class Container>
template<bool IsConst>
class packed_view<Container>::range_reference_base
{
    friend class packed_view;
    
    using PV = typename std::conditional<IsConst, packed_view const,
                                                  packed_view>::type;
    
    range_reference_base(PV &v, size_t begin, size_t end)
        : _v(v), _begin(begin), _end(end) { }
public:
    range_reference_base(range_reference_base const&r) = default;
    range_reference_base &operator=(range_reference_base const&) = delete;
    
    packed_view::value_type value() const {
        return _v._bits.get(_begin * _v._width, _end * _v._width);
    }
    
    operator packed_view::value_type() const {
        return value();
    }
    
private:
    PV &_v;
    size_t _begin;
    size_t _end;
};

#endif
