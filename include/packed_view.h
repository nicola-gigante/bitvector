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

#include "bits.h"

#include <vector>
#include <type_traits>

namespace bitvector
{
    /*
     * This class is an adapter that transform a container of elements of W bits,
     * into collection of contiguous elements of different and arbitrary bit size.
     *
     * The constructor takes the width in bits of the elements and the number
     * of elements to present, in addition to any argument suitable to
     * construct the underlying container.
     *
     * The class provides a container-like interface, including random access
     * iterators and operator[].
     *
     * The default argument for the ContainerT template parameter is array_view.
     * With this configuration the class behaves like a non-owning view on the
     * data pointed by the array_view, and the constructor then takes the
     * pointer to the data buffer and the length.
     *
     */
    template<size_t W, template<typename...> class ContainerT = array_view>
    class packed_view
    {
    protected:
        using Container = ContainerT<word_t<W>>;
        
    public:
        class reference;
        class const_reference;

        template<bool>
        class iterator_t;

        using iterator               = iterator_t<false>;
        using const_iterator         = iterator_t<true>;
        using pointer                = iterator;
        using const_pointer          = const_iterator;
        using value_type             = word_t<W>;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using size_type              = size_t;
        using difference_type        = decltype(size_t() - size_t());
        
        template<typename ...Args,
                 REQUIRES(std::is_constructible<Container, Args...>::value)>
        packed_view(size_t width, size_t size, Args&& ...args)
            : _container(std::forward<Args>(args)...), _width(width), _size(size)
        {
            assert(width > 0);
            assert(size > 0);
            assert(width <= W);
            assert(width * size <= W * _container.size());
        }
        
        packed_view(size_t width, size_t size)
            : packed_view(width, size, required_length(width, size)) { }
        
        packed_view(packed_view const&) = default;
        packed_view(packed_view &&) = default;
        
        packed_view &operator=(packed_view const&) = default;
        packed_view &operator=(packed_view &&) = default;
        
        // The number of presented elements
        size_t size() const { return _size; }
        
        Container const&container() const {
            return _container;
        }
        
        Container &container() {
            return _container;
        }
        
        const_reference at(size_t i) const {
            assert(i < size());
            return { *this, i };
        }
        
        reference at(size_t i) {
            assert(i < size());
            return { *this, i };
        }
        
        template<typename C = Container,
                 typename = decltype(std::declval<C>().resize(42))>
        void resize(size_t width, size_t size)
        {
            assert(width > 0);
            assert(size > 0);
            assert(width <= W);
            
            _width = width;
            _size = size;
            container().resize(required_length(width, size));
        }
        
        
        const_reference operator[](size_t i) const { return at(i); }
        reference       operator[](size_t i)       { return at(i); }
        
        iterator       begin()       { return { *this, 0 }; }
        const_iterator begin() const { return { *this, 0 }; }
        
        iterator       end()       { return { *this, _size }; }
        const_iterator end() const { return { *this, _size }; }
        
        reverse_iterator       rbegin()       { return { end() }; }
        const_reverse_iterator rbegin() const { return { end() }; }
        
        reverse_iterator       rend()       { return { begin() }; }
        const_reverse_iterator rend() const { return { begin() }; }
        
        const_iterator cbegin() const { return { *this, 0 }; }
        const_iterator cend() const { return { *this, _size }; }
        const_reverse_iterator crbegin() const { return { end() }; }
        const_reverse_iterator crend() const { return { begin() }; }
        
    private:
        // Suggests the length of a buffer required to store the specified
        // number of elements of the specified width
        size_t required_length(size_t width, size_t size)
        {
            return size_t(std::ceil((float(width) * size)/W));
        }
        
        std::tuple<size_t, size_t, size_t, size_t>
        locate(size_t index) const
        {
            size_t i = (_width * index) / W; /// Word's index into the array
            size_t l = (_width * index) % W; /// First bit's index into the word
            
            /// Length of the low part
            size_t llen = std::min(W - l, size_t(_width));
            size_t hlen = _width - llen; /// Length of the high part
            
            return { i, l, llen, hlen };
        }
        
        word_t<W> get(size_t index) const
        {
            assert(index < _size);
            
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(index);
            
            word_t<W> low  = get_bitfield(_container[i], l, l + llen);
            word_t<W> high = get_bitfield(_container[i + 1], 0, hlen) << llen;
            
            return high | low;
        }
        
        void set(size_t index, word_t<W> value)
        {
            assert(index < _size);
            
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(index);
            
            set_bitfield(&_container[i], l, l + llen, value);
            set_bitfield(&_container[i + 1], 0, hlen, value >> llen);
        }

    private:
        Container _container;
        
        size_t _width; // Number of bits per element
        size_t _size; // Number of elements
    };
    
    template<size_t W, template<typename...> class ContainerT>
    class packed_view<W, ContainerT>::reference
    {
        friend class packed_view;
        
        packed_view &_v;
        size_t _index;
        
        reference(packed_view &v, size_t index) : _v(v), _index(index) { }
    public:
        operator word_t<W>() const {
            return _v.get(_index);
        }
        
        reference &operator=(word_t<W> value)
        {
            _v.set(_index, value);
            
            return *this;
        }
    };
    
    template<size_t W, template<typename...> class ContainerT>
    class packed_view<W, ContainerT>::const_reference
    {
        friend class packed_view;
        
        packed_view const&_v;
        size_t _index;
        
        const_reference(packed_view const&v, size_t index)
            : _v(v), _index(index) { }
    public:
        operator word_t<W>() const {
            return _v.get(_index);
        }
    };
    
    template<size_t W, template<typename...> class ContainerT>
    template<bool Const>
    class packed_view<W, ContainerT>::iterator_t
    {
        friend class packed_view;
    public:
        using difference_type   = packed_view::difference_type;
        using value_type        = packed_view::value_type;
        using pointer           = iterator_t;
        using iterator_category = std::random_access_iterator_tag;
        
        iterator_t() = default;
        iterator_t(std::nullptr_t) { }
        
        iterator_t(packed_view &v, size_t i) : _v(&v), _i(i) { }
        iterator_t(packed_view const&v, size_t i)
            : _v(const_cast<packed_view*>(&v)), _i(i) { }
        
        iterator_t(iterator const&) = default;
        iterator_t(iterator &&) = default;
        
        iterator_t &operator=(iterator_t const&) = default;
        iterator_t &operator=(iterator_t &&) = default;
        
        // Access
        const_reference operator*() const {
            return _v->at(_i);
        }
        
        reference operator*() {
            return _v->at(_i);
        }
        
        const_reference operator[](size_t i) const {
            return _v->at(_i + i);
        }
        
        reference operator[](size_t i) {
            return _v->at(_i + i);
        }
        
        // Equality and Comparisons
        bool operator==(iterator_t const&it) const {
            return _v == it._v && _i == it._i;
        }
        
        bool operator!=(iterator_t const&it) const { return !(*this == it); }
        bool operator<(iterator_t const&it) const { return _i < it._i; }
        bool operator>(iterator_t const&it) const { return _i > it._i; }
        bool operator<=(iterator_t const&it) const { return _i <= it._i; }
        bool operator>=(iterator_t const&it) const { return _i >= it._i; }
        
    
        // Increments and decrements
        iterator &operator++() {
            _i++;
            
            return *this;
        }
        
        iterator operator++(int) const {
            iterator t = *this;
            ++_i;
            
            return t;
        }
        
        iterator &operator--() {
            --_i;
            
            return *this;
        }
        
        iterator operator--(int) const {
            iterator t = *this;
            --_i;
            
            return t;
        }
        
        // Sum and difference
        iterator &operator+=(difference_type n) {
            _i += n;
            
            return *this;
        }
        
        iterator operator+(difference_type n) const {
            return { *_v, _i + n };
        }
        
        friend iterator operator+(difference_type n, iterator const&it) {
            return { *it._v, it._i + n };
        }
        
        iterator &operator-=(difference_type n) {
            _i -= n;
            
            return *this;
        }
        
        iterator operator-(difference_type n) const {
            return { *_v, _i - n };
        }
        
        friend difference_type operator-(iterator const&it1, iterator const&it2) {
            return it1._i - it2._i;
        }
        
    private:
        packed_view *_v = nullptr;
        size_t _i = 0;
    };
    
    template<size_t W>
    using packed_array = packed_view<W, std::vector>;
}
#endif


