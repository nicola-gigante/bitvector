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
    enum flag_bit_t : bool
    {
        no_flag_bit = false,
        flag_bit = true
    };
    
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
    template<size_t W, template<typename...> class ContainerT = array_view,
             flag_bit_t FlagBit = no_flag_bit>
    class packed_view
    {
    protected:
        using Container = ContainerT<word_t<W>>;
        
        template<bool>
        class iterator_t;

        template<bool>
        class reference_base;
        
    public:
        class reference;
        using const_reference = reference_base<false>;

        using iterator               = iterator_t<false>;
        using const_iterator         = iterator_t<true>;
        using pointer                = iterator;
        using const_pointer          = const_iterator;
        using value_type             = word_t<W>;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using size_type              = size_t;
        using difference_type        = decltype(size_t() - size_t());
        
        packed_view() = default;
        
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
        
        // Width of presented elements
        size_t width() const { return _width; }
        
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
        
        const_reference at(size_t begin, size_t end) const {
            assert(begin < size());
            assert(end <= size());
            assert(_width * (end - begin) <= W);
            
            return { *this, begin, end };
        }
        
        reference at(size_t begin, size_t end) {
            assert(begin < size());
            assert(end <= size());
            assert(_width * (end - begin) <= W);
            
            return { *this, begin, end };
        }
        
        template<typename C = Container,
                 typename = decltype(std::declval<C>().reserve(42),
                                     std::declval<C>().resize(42))>
        void resize(size_t width, size_t size)
        {
            assert(width > 0);
            assert(size > 0);
            assert(width <= W);
            
            _width = width;
            _size = size;
            size_t length = required_length(width, size);
            
            container().reserve(length);
            container().resize(length);
        }
        
        const_reference operator[](size_t i) const { return at(i); }
        reference       operator[](size_t i)       { return at(i); }
        
        const_reference operator()(size_t begin, size_t end) const {
            return at(begin, end);
        }
        
        reference       operator()(size_t begin, size_t end) {
            return at(begin, end);
        }
        
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
        locate(size_t begin, size_t end) const
        {
            size_t len = _width * (end - begin);
            assert(len <= W);
            
            size_t i = (_width * begin) / W; /// Word's index into the array
            size_t l = (_width * begin) % W; /// First bit's index into the word
            
            /// Length of the low part
            size_t llen = std::min(W - l, len);
            size_t hlen = len - llen; /// Length of the high part
            
            return { i, l, llen, hlen };
        }
        
        word_t<W> get(size_t index) const
        {
            return get(index, index + 1);
        }
        
        word_t<W> get(size_t begin, size_t end) const
        {
            assert(begin < _size);
            assert(end <= _size);
            
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(begin, end);
            
            word_t<W> low  = get_bitfield(_container[i], l, l + llen);
            word_t<W> high = get_bitfield(_container[i + 1], 0, hlen) << llen;
            
            return high | low;
        }
        
        void set(size_t index, word_t<W> value)
        {
            set(index, index + 1, value);
        }
        
        void set(size_t begin, size_t end, word_t<W> value)
        {
            assert(begin < _size);
            assert(end <= _size);
            
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(begin, end);
            
            set_bitfield(&_container[i], l, l + llen, value);
            set_bitfield(&_container[i + 1], 0, hlen, value >> llen);
        }
        
    private:
        Container _container;
        
        size_t _width = 0; // Number of bits per element
        size_t _size = 0; // Number of elements
    };
    
    template<size_t W, template<typename...> class ContainerT, flag_bit_t FlagBit>
    template<bool Const>
    class packed_view<W, ContainerT, FlagBit>::reference_base
    {
        friend class packed_view;
        
        using PV = typename std::conditional<Const, packed_view const,
                                                    packed_view>::type;
        
        reference_base(PV &v, size_t begin, size_t end)
            : _v(v), _begin(begin), _end(end) { }
        
        reference_base(PV &v, size_t begin)
            : _v(v), _begin(begin), _end(begin + 1) { }
        
        reference_base(reference_base const&r) = default;
        
        template<bool C, REQUIRES(not C)>
        reference_base(reference_base<C> const&r)
            : _v(r._v), _begin(r._begin), _end(r._end) { }
        
    protected:
        word_t<W> flag_bitmask() const {
            return (FlagBit & (_end == _begin + 1)) << (_v.width() - 1);
        }
        
    public:
        word_t<W> value() const {
            return _v.get(_begin, _end) & ~flag_bitmask();
        }
        
        operator word_t<W>() const {
            return value();
        }
        
    protected:
        PV &_v;
        size_t _begin;
        size_t _end;
    };
    
    template<size_t W, template<typename...> class ContainerT, flag_bit_t FlagBit>
    class packed_view<W, ContainerT, FlagBit>::reference
        : public reference_base<false>
    {
        friend class packed_view;
        
        using Base = reference_base<false>;
        
        using Base::Base;
        
    public:
        reference &operator=(word_t<W> v)
        {
            v = v | Base::flag_bitmask();
            Base::_v.set(Base::_begin, Base::_end, v);
            
            return *this;
        }
        
        reference &operator +=(word_t<W> v)
        {
            *this = *this + v;
            return *this;
        }
        
        reference &operator -=(word_t<W> v)
        {
            *this = *this - v;
            return *this;
        }
        
        reference &operator>>=(size_t n)
        {
            assert(n < W);
            
            *this = *this >> n;
            return *this;
        }
        
        reference &operator<<=(size_t n)
        {
            assert(n < W);
            
            *this = *this << n;
            return *this;
        }
    };
    
    template<size_t W, template<typename...> class ContainerT, flag_bit_t FlagBit>
    template<bool Const>
    class packed_view<W, ContainerT, FlagBit>::iterator_t
    {
        friend class packed_view;
        using PV = typename std::conditional<Const, packed_view const,
                                                    packed_view>::type;
        
    public:
        using difference_type   = packed_view::difference_type;
        using value_type        = packed_view::value_type;
        using pointer           = iterator_t;
        using iterator_category = std::random_access_iterator_tag;
        
        using reference = typename
                          std::conditional<Const, packed_view::const_reference,
                                                  packed_view::reference>::type;
        
        iterator_t() = default;
        iterator_t(std::nullptr_t) { }
        
        iterator_t(PV &v, size_t i) : _v(&v), _i(i) { }
        
        iterator_t(iterator const&) = default;
        iterator_t(iterator &&) = default;
        
        iterator_t &operator=(iterator_t const&) = default;
        iterator_t &operator=(iterator_t &&) = default;
        
        packed_view const&view() const { return _v; }
        size_t index() const { return _i; }
        
        // Access
        reference operator*() const {
            return _v->at(_i);
        }
        
        reference operator[](size_t i) const {
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
        PV *_v = nullptr;
        size_t _i = 0;
    };
    
    template<size_t W, flag_bit_t FlagBit = no_flag_bit>
    using packed_array = packed_view<W, std::vector, FlagBit>;
}
#endif


