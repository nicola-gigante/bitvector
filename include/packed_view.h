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

enum flag_bit_t : bool
{
    no_flag_bit = false,
    flag_bit = true
};

template<template<typename ...> class Container,
         flag_bit_t FlagBit = no_flag_bit>
class packed_view
{
    
public:
    using value_type = typename bitview<Container>::value_type;
    using container_type = typename bitview<Container>::container_type;

    class range_reference;
    class item_reference;
    class const_item_reference;
    
    packed_view() = default;
    
    packed_view(size_t width, size_t size)
        : _bits(width * size), _width(width),
          _field_mask(compute_field_mask(width)) { }
    
    packed_view(packed_view const&) = default;
    packed_view(packed_view &&) = default;
    packed_view &operator=(packed_view const&) = default;
    packed_view &operator=(packed_view &&) = default;
    
    container_type const&container() const { return _bits.container(); }
    container_type      &container()       { return _bits.container(); }
    
    // The number of fields contained in the packed_view
    size_t size() const { return _bits.size() / _width; }
    
    // The number of bits for each field
    size_t width() const { return _width; }
    
    value_type field_mask() const { return _field_mask; }
    value_type flag_mask() const { return _field_mask << (_width - 1); }
    
    // Sets the number of bits for each field.
    // Warning: This is a disruptive operation.
    // No attempts of preserving any meaningful data are make.
    void set_width(size_t width) {
        size_t oldsize = size();
        _width = width;
        _field_mask = compute_field_mask(width);
        resize(oldsize);
    }
    
    // Change the size of the view, possibly changing the size of the underlying
    // container as well
    void resize(size_t size) {
        _bits.resize(_width * size);
    }
    
    range_reference operator()(size_t begin, size_t end) {
        return { *this, begin, end };
    }
    
    item_reference operator[](size_t index) {
        assert(index < size());
        return { *this, index };
    }
    
    const_item_reference operator[](size_t index) const {
        assert(index < size());
        return { *this, index };
    }
    
private:
    static value_type compute_field_mask(size_t width);
    
private:
    // Actual data
    bitview<Container> _bits;
    
    // Number of bits per field
    size_t _width;
    
    // Mask with a set bit at the beginning of each field
    value_type _field_mask;
};

template<template<typename ...> class Container, flag_bit_t FlagBit>
auto packed_view<Container, FlagBit>::compute_field_mask(size_t width) -> value_type
{
    size_t degree = bitview<Container>::W / width;
    value_type mask = 0;
    
    for(size_t i = 0; i < degree; ++i) {
        mask <<= width;
        mask |= 1;
    }
        
    return mask;
}
       
template<template<typename ...> class Container, flag_bit_t FlagBit>
class packed_view<Container, FlagBit>::range_reference
{
    friend class packed_view;
    
    range_reference(packed_view &v, size_t begin, size_t end)
        : _v(v), _begin(begin), _end(end) { }
    
public:
    range_reference(range_reference const&) = default;
    
    range_reference const&operator=(value_type value) const
    {
        _v._bits.set(_begin * _v._width,
                     _end   * _v._width,
                     value);
        
        return *this;
    }
    
    range_reference const&operator+=(range_reference const&ref) const
    {
        _v._bits.set_sum(ref._v._bits, ref._begin, ref._end, _begin);
        
        return *this;
    }
    
private:
    packed_view &_v;
    size_t _begin;
    size_t _end;
};

template<template<typename ...> class Container, flag_bit_t FlagBit>
class packed_view<Container, FlagBit>::const_item_reference
{
    friend class packed_view;
    
    const_item_reference(packed_view const&v, size_t index)
        : _v(v), _index(index) { }
    
public:
    const_item_reference(const_item_reference const&) = default;
    
    value_type value() const
    {
        size_t begin = _index * _v.width();
        size_t end = begin + _v.width();
        
        return _v._bits.get(begin, end);
    }
    
    operator value_type() const {
        return value();
    }
    
private:
    packed_view const&_v;
    size_t _index;
};
        
template<template<typename ...> class Container, flag_bit_t FlagBit>
class packed_view<Container, FlagBit>::item_reference
{
    friend class packed_view;
    
    item_reference(packed_view &v, size_t index)
        : _v(v), _index(index) { }
    
public:
    item_reference(item_reference const&) = default;
    
    operator const_item_reference() const {
        return { _v, _index };
    }
    
    value_type value() const {
        return const_item_reference(*this).value();
    }
    
    operator value_type() const {
        return value();
    }
    
    item_reference const&operator=(value_type v) const {
        size_t begin = _index * _v.width();
        size_t end   = begin + _v.width();
        
        _v._bits.set(begin, end, v);
        
        return *this;
    }
    
private:
    packed_view &_v;
    size_t _index;
};
        
#endif
