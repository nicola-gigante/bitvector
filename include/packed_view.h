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

#ifndef PACKED_BITVECTOR_PACKED_VIEW_H
#define PACKED_BITVECTOR_PACKED_VIEW_H

#include "bitview.h"
#include "internal/bits.h"
#include "internal/view_reference.h"

#include <string>

namespace bv
{
    namespace internal {
        template<template<typename ...> class Container>
        class packed_view
        {
            static constexpr size_t W = bitview<Container>::W;
            
            template<bool IsConst>
            class packed_iterator;
            
        public:
            using word_type  = typename bitview<Container>::word_type;
            using value_type = word_type;
            
            using range_reference        = internal::range_reference<packed_view>;
            using const_range_reference  = internal::const_range_reference<packed_view>;
            using reference              = internal::item_reference<packed_view>;
            using const_reference        = internal::const_item_reference<packed_view>;
            using difference_type        = ssize_t;
            using const_pointer          = packed_iterator<true>;
            using pointer                = packed_iterator<false>;
            using const_iterator         = const_pointer;
            using iterator               = pointer;
            using reverse_iterator       = std::reverse_iterator<iterator>;
            using reverse_const_iterator = std::reverse_iterator<const_iterator>;
            
            using container_type = typename bitview<Container>::container_type;
            
            packed_view() = default;
            
            packed_view(size_t width, size_t size)
                : _bits(width * size), _size(size), _width(width),
                  _field_mask(compute_field_mask(width)) { }
            
            packed_view(packed_view const&) = default;
            packed_view(packed_view &&) = default;
            packed_view &operator=(packed_view const&) = default;
            packed_view &operator=(packed_view &&) = default;
            
            container_type const&container() const { return _bits.container(); }
            container_type      &container()       { return _bits.container(); }
            
            bitview<Container> const&bits() const { return _bits; }
            bitview<Container>      &bits()       { return _bits; }
            
            // A default initialized packed_view is empty
            bool empty() const { return _bits.empty() || _width == 0; }
            
            // The number of fields presented by the packed_view
            size_t size() const {
                return _size;
            }
            
            // The number of fields that can fit in the underlying bitview
            size_t capacity() const {
                return empty() ? 0 : _bits.size() / _width;
            }
            
            // The number of bits for each field
            size_t width() const { return _width; }
            
            word_type field_mask() const { return _field_mask; }
            word_type flag_mask() const { return _field_mask << (_width - 1); }
            
            /*
             * Change the size of the view, possibly changing the size of the
             * underlying container as well
             */
            void resize(size_t size) {
                _size = size;
                _bits.resize(_width * size);
            }
            
            /*
             * Resets the container's size and the number of bits for each field
             * Warning: No attempts to preserve the data are made, although in
             *          practice the data doesn't get touched
             */
            void reset(size_t width, size_t size) {
                assert(size == 0 || width != 0);
                
                if(width != _width) {
                    _width = width;
                    _field_mask = compute_field_mask(width);
                }
                resize(size);
            }
            
            /*
             * Interface to the reference wrapper.
             * It is recommended to use the high-level interface instead
             */
            size_t elements_per_word() const { return W / width(); }
            
            word_type get(size_t begin, size_t end) const;
            void set(size_t begin, size_t end, word_type value);
            
            value_type get(size_t index) const;
            void set(size_t index, value_type value);
            
            void increment(size_t begin, size_t end, size_t n);
            void decrement(size_t begin, size_t end, size_t n);
            
            template<template<typename ...> class C>
            void copy(packed_view<C> const&src,
                      size_t src_begin, size_t src_end,
                      size_t dest_begin, size_t dest_end);
            
            std::string to_binary(size_t begin, size_t end,
                                  size_t sep, char ssep) const;
            
            
            /*
             * High-level access interface to the view's contents
             */
            range_reference operator()(size_t begin, size_t end) {
                return { *this, begin, end };
            }
            
            const_range_reference operator()(size_t begin, size_t end) const {
                return { *this, begin, end };
            }
            
            reference operator[](size_t index) {
                assert(index < size());
                return { *this, index };
            }
            
            const_reference operator[](size_t index) const {
                assert(index < size());
                return { *this, index };
            }
            
            iterator begin() { return iterator(this, 0); }
            iterator end()   { return iterator(this, _size); }
            
            const_iterator begin() const { return const_iterator(this, 0); }
            const_iterator end()   const { return const_iterator(this, _size); }
            
            const_iterator cbegin() const { return begin(); }
            const_iterator cend()   const { return end();   }
            
            reverse_iterator rbegin() { return reverse_iterator(end()); }
            reverse_iterator rend()   { return reverse_iterator(begin()); }
            
            reverse_const_iterator rbegin() const {
                return reverse_const_iterator(cend());
            }
            
            reverse_const_iterator rend()   const {
                return reverse_const_iterator(cbegin());
            }
            
            reverse_const_iterator crbegin() const { return rbegin(); }
            reverse_const_iterator crend()   const { return rend();   }
            
        private:
            /*
             * Private details
             */
            static word_type compute_field_mask(size_t width);
            
            enum overflow_opt {
                may_overflow,
                cant_overflow
            };
            
            void increment(size_t begin, size_t end, size_t n, overflow_opt opt);
            size_t find(size_t begin, size_t end, word_type value) const;
            
        private:
            // Actual data
            bitview<Container> _bits;
            
            // Number of requested fields
            size_t _size = 0;
            
            // Number of bits per field
            size_t _width = 0;
            
            // Mask with a set bit at the beginning of each field
            word_type _field_mask = 0;
        };
        
        template<template<typename ...> class C>
        typename packed_view<C>::word_type
        packed_view<C>::compute_field_mask(size_t width)
        {
            size_t fields_per_word = W / width;
            word_type mask = 0;
            
            for(size_t i = 0; i < fields_per_word; ++i)
            {
                mask = mask << width;
                mask = mask | 1;
            }
            
            return mask;
        }
        
        template<template<typename ...> class C>
        typename packed_view<C>::word_type
        packed_view<C>::get(size_t begin, size_t end) const
        {
            return _bits.get(begin * width(), end * width());
        }
        
        template<template<typename ...> class C>
        typename packed_view<C>::value_type
        packed_view<C>::get(size_t index) const
        {
            return get(index, index + 1);
        }
        
        template<template<typename ...> class C>
        void packed_view<C>::set(size_t begin, size_t end, word_type pattern)
        {
            size_t fields_per_word = W / width();
            size_t bits_per_word = fields_per_word * width(); // It's not useless
            size_t len = (end - begin) * width();
            size_t rem = len % bits_per_word;
            
            ensure_bitsize(pattern, width());
            
            word_type value = field_mask() * pattern;
            
            for(size_t step, p = begin * width();
                p < end * width();
                len -= step, p += step)
            {
                step = len < bits_per_word ? rem : bits_per_word;
                
                _bits.set(p, p + step, lowbits(value, step));
            }
        }
        
        template<template<typename ...> class C>
        void packed_view<C>::set(size_t index, value_type value)
        {
            _bits.set(index * width(), (index + 1) * width(), value);
        }
        
        template<template<typename ...> class C>
        void packed_view<C>::increment(size_t begin, size_t end,
                                       size_t n, overflow_opt opt)
        {
            size_t fields_per_word = W / width();
            size_t bits_per_word = fields_per_word * width(); // It's not useless
            size_t len = (end - begin) * width();
            size_t rem = len % bits_per_word;
            
            for(size_t step, p = begin * width();
                p < end * width();
                len -= step, p += step)
            {
                step = len < bits_per_word ? rem : bits_per_word;
                
                word_type result = _bits.get(p, p + step) +
                lowbits(field_mask() * n, step);
                
                if(opt == cant_overflow)
                    ensure_bitsize(result, step);
                
                _bits.set(p, p + step, lowbits(result, step));
            }
        }
        
        template<template<typename ...> class C>
        void packed_view<C>::increment(size_t begin, size_t end, size_t n)
        {
            increment(begin, end, n, cant_overflow);
        }

        template<template<typename ...> class C>
        void packed_view<C>::decrement(size_t begin, size_t end, size_t n)
        {
            increment(begin, end, - n, may_overflow);
        }
        
        template<template<typename ...> class C1>
        template<template<typename ...> class C2>
        void packed_view<C1>::copy(packed_view<C2> const&src,
                                  size_t src_begin, size_t src_end,
                                  size_t dest_begin, size_t dest_end)
        {
            _bits.copy(src._bits,
                       src_begin  * src.width(),
                       src_end    * src.width(),
                       dest_begin *     width(),
                       dest_end   *     width());
        }
        
        template<template<typename ...> class C>
        std::string packed_view<C>::to_binary(size_t begin, size_t end,
                                              size_t sep, char ssep) const
        {
            return bits().to_binary(begin * width(), end * width(),
                                    sep, ssep);
        }
    
        template<template<typename ...> class C>
        template<bool IsConst>
        class packed_view<C>::packed_iterator {
            friend class packed_view;
            
            using view_t = typename
                std::conditional<IsConst, packed_view const, packed_view>::type;
            
            view_t *_v = nullptr;
            size_t _index = 0;
            
            packed_iterator(view_t *v, size_t index)
                : _v(v), _index(index) { }

        public:
            using iterator_category = std::random_access_iterator_tag;
            using difference_type   = typename view_t::difference_type;
            using value_type        = typename view_t::value_type;
            using pointer           = packed_iterator;
            using reference         = typename
                std::conditional<IsConst,
                                 typename view_t::const_reference,
                                 typename view_t::reference>::type;
            
            packed_iterator() = default;
            packed_iterator(packed_iterator const&) = default;
            
            template<typename T = void,
                     REQUIRES(std::is_same<T,T>::value && IsConst)>
            packed_iterator(packed_iterator<false> const&it)
                : _v(it._v), _index(it._index) { }
            
            packed_iterator &operator=(packed_iterator const&) = default;
            
            // Iterator
            reference operator*() const {
                return (*_v)[_index];
            }
            
            packed_iterator &operator++() {
                ++_index;
                return *this;
            }
            
            // EqualityComparable
            bool operator==(packed_iterator it) const {
                return _index == it._index;
            }
            
            // ForwardIterator
            bool operator!=(packed_iterator it) const {
                return _index != it._index;
            }
            
            packed_iterator operator++(int) {
                packed_iterator it(*this);
                ++_index;
                return it;
            }
            
            // BidirectionalIterator
            packed_iterator &operator--() {
                --_index;
                return *this;
            }
            
            packed_iterator operator--(int) {
                packed_iterator it(*this);
                --_index;
                return it;
            }
            
            // RandomAccessIterator
            packed_iterator &operator+=(difference_type n) {
                _index += n;
                return *this;
            }
            
            packed_iterator operator+(difference_type n) const {
                packed_iterator it(*this);
                
                it += n;
                
                return it;
            }
            
            friend packed_iterator operator+(difference_type n,
                                             packed_iterator it)
            {
                return it + n;
            }
            
            packed_iterator &operator-=(difference_type n) {
                _index -= n;
                return *this;
            }
            
            packed_iterator operator-(difference_type n) const {
                packed_iterator it(*this);
                
                it -= n;
                
                return it;
            }
            
            friend packed_iterator operator-(difference_type n,
                                             packed_iterator it) {
                return it - n;
            }
            
            difference_type operator-(packed_iterator it) {
                return difference_type(_index) - difference_type(it._index);
            }
            
            reference operator[](difference_type i) const {
                return (*_v)[_index + i];
            }
            
            bool operator<(packed_iterator it) const {
                return _index < it._index;
            }
            
            bool operator<=(packed_iterator it) const {
                return _index <= it._index;
            }
            
            bool operator>(packed_iterator it) const {
                return _index > it._index;
            }
            
            bool operator>=(packed_iterator it) const {
                return _index >= it._index;
            }
        };
    } // namespace internal
    
    // Public thing
    using internal::packed_view;
    
} // namespace bv

#endif
