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
#include "bits.h"

#include <string>

namespace bitvector
{
    template<template<typename ...> class Container>
    class packed_view
    {
        static constexpr size_t W = bitview<Container>::W;
    public:
        using value_type = typename bitview<Container>::value_type;
        using container_type = typename bitview<Container>::container_type;
        
        class range_reference;
        class const_range_reference;
        class item_reference;
        class const_item_reference;
        
        friend class range_reference;
        
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
        
        /*
         * Resets the container's size and the number of bits for each field
         * Warning: No attempts to preserve the data are made, although in
         *          practice the data doesn't get touched
         */
        void reset(size_t width, size_t size) {
            _width = width;
            _field_mask = compute_field_mask(width);
            resize(size);
        }
        
        /*
         * Change the size of the view, possibly changing the size of the
         * underlying container as well
         */
        void resize(size_t size) {
            _bits.resize(_width * size);
        }
        
        range_reference operator()(size_t begin, size_t end) {
            return { *this, begin, end };
        }
        
        const_range_reference operator()(size_t begin, size_t end) const {
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
        
        value_type get(size_t begin, size_t end) const;
        void set(size_t begin, size_t end, value_type value);
        
        void repeat(size_t begin, size_t end, value_type value);
        size_t find(size_t begin, size_t end, value_type value) const;
        static std::string to_binary(const_range_reference const&ref,
                                     size_t sep, char ssep);
        
    private:
        // Actual data
        bitview<Container> _bits;
        
        // Number of bits per field
        size_t _width;
        
        // Mask with a set bit at the beginning of each field
        value_type _field_mask;
    };
    
    template<template<typename ...> class Container>
    auto packed_view<Container>::compute_field_mask(size_t width) -> value_type
    {
        size_t fields_per_word = W / width;
        value_type mask = 0;
        
        for(size_t i = 0; i < fields_per_word; ++i)
        {
            mask = mask << width;
            mask = mask | 1;
        }
            
        return mask;
    }
            
    template<template<typename ...> class C>
    auto packed_view<C>::get(size_t begin, size_t end) const -> value_type
    {
        return _bits.get(begin * width(), end * width());
    }
    
    template<template<typename ...> class Container>
    void packed_view<Container>::set(size_t begin, size_t end, value_type value)
    {
        return _bits.set(begin * width(), end * width(), value);
    }
    
    template<template<typename ...> class C>
    void packed_view<C>::repeat(size_t begin, size_t end, value_type pattern)
    {
        size_t fields_per_word = W / width();
        size_t bits_per_word = fields_per_word * width(); // It's not useless
        size_t len = (end - begin) * width();
        size_t rem = len % bits_per_word;
        
        value_type value = field_mask() * lowbits(pattern, width());
        
        for(size_t step, p = begin * width();
            p < end * width();
            len -= step, p += step)
        {
            step = len < bits_per_word ? rem : bits_per_word;
            
            _bits.set(p, p + step, value);
        }
    }
    
    template<template<typename ...> class Container>
    size_t packed_view<Container>::find(size_t begin, size_t end,
                                        value_type value) const
    {
        size_t fields_per_word = W / width();
        size_t len = end - begin;
        size_t rem = len % fields_per_word;
        
        value = field_mask() * lowbits(value, width() - 1);
        
        size_t result = len;
        for(size_t step, p = begin; p < end; len -= step, p += step)
        {
            step = len < fields_per_word ? rem : fields_per_word;
            
            value_type word = get(p, p + step) | flag_mask();
            
            result -= popcount(lowbits(flag_mask() & (word - value),
                                       step * width()));
        }
        
        return result;
    }
    
    template<template<typename ...> class C>
    std::string packed_view<C>::to_binary(const_range_reference const&ref,
                                          size_t sep, char ssep)
    {
        std::string s;
        
        size_t begin = ref._begin * ref._v.width();
        size_t end = ref._end * ref._v.width();
        
        // It is slooooow. Oh well.. it's debugging output after all...
        for(size_t i = begin, bits = 0; i < end; ++i, ++bits) {
            if(bits && bits % sep == 0)
                s += ssep;
            s += ref._v._bits.get(i) ? '1' : '0';
        }
        
        std::reverse(s.begin(), s.end());
        
        return s;
    }
    
    template<template<typename ...> class Container>
    class packed_view<Container>::const_range_reference
    {
        friend class packed_view;
        
        const_range_reference(packed_view const&v, size_t begin, size_t end)
            : _v(v), _begin(begin), _end(end) { }
        
    public:
        const_range_reference(const_range_reference const&) = default;
        
        size_t find(value_type value) const
        {
            return _v.find(_begin, _end, value);
        }
        
        friend std::string to_binary(const_range_reference const&ref,
                                     size_t sep = 8, char ssep = ' ')
        {
            return packed_view::to_binary(ref, sep, ssep);
        }
        
    private:
        packed_view const&_v;
        size_t _begin;
        size_t _end;
    };
    
    template<template<typename ...> class Container>
    class packed_view<Container>::range_reference
    {
        friend class packed_view;
        
        range_reference(packed_view &v, size_t begin, size_t end)
            : _v(v), _begin(begin), _end(end) { }
        
    public:
        range_reference(range_reference const&) = default;
        
        operator const_range_reference() const {
            return { _v, _begin, _end };
        }
        
        range_reference const&operator=(value_type value) const
        {
            _v.repeat(_begin, _end, value);
            
            return *this;
        }
        
        range_reference const&operator=(const_range_reference const&ref) const
        {
            _v._bits.copy(ref._v._bits, ref._begin, ref._end, _begin);
            
            return *this;
        }
        
        range_reference const&operator=(range_reference const&ref) const
        {
            operator=(const_range_reference(*this));
            
            return *this;
        }
        
        range_reference const&operator+=(const_range_reference const&ref) const
        {
            _v._bits.set_sum(ref._v._bits, ref._begin, ref._end, _begin);
            
            return *this;
        }
        
        size_t find(value_type value) const {
            return const_range_reference(*this).find(value);
        }
        
        friend std::string to_binary(range_reference const&ref,
                                     size_t sep = 8, char ssep = ' ')
        {
            return packed_view::to_binary(ref, sep, ssep);
        }
        
    private:
        packed_view &_v;
        size_t _begin;
        size_t _end;
    };
    
    template<template<typename ...> class Container>
    class packed_view<Container>::const_item_reference
    {
        friend class packed_view;
        
        const_item_reference(packed_view const&v, size_t index)
            : _v(v), _index(index) { }
        
    public:
        const_item_reference(const_item_reference const&) = default;
        
        value_type value() const {
            return _v.get(_index, _index + 1);
        }
        
        operator value_type() const {
            return value();
        }
        
    private:
        packed_view const&_v;
        size_t _index;
    };
    
    
    template<template<typename ...> class Container>
    class packed_view<Container>::item_reference
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
            _v.set(_index, _index + 1, v);
            
            return *this;
        }
        
    private:
        packed_view &_v;
        size_t _index;
    };
    
} // namespace bitvector

#endif
