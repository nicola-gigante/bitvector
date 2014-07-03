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

#define REQUIRES(...) \
typename = typename std::enable_if<(__VA_ARGS__)>::type

namespace bitvector
{
    /*
     * This class is a non-owning view on a buffer of elements of W bits,
     * which presents a collection of contiguous elements of different size.
     *
     * The constructor takes the buffer and the length of the buffer,
     * and then the number of bits of the presented elements and the number
     * of such elements.
     *
     * Then, with operator[] you can access and modify individual elements,
     * and the view puts the bits in the right places.
     */
    template<size_t W, template<typename T> class Base>
    class packed_view : protected Base<word_t<W>>
    {
    protected:
        using Container = Base<word_t<W>>;
        
        Container      &super()       { return static_cast<Container      &>(*this); }
        Container const&super() const { return static_cast<Container const&>(*this); }
        
        template<bool Const>
        class reference_t
        {
            friend class packed_view;
            
            using P = typename std::conditional<Const,
                                                packed_view const,
                                                packed_view>::type;
            
            P &_v;
            size_t _index;
            
            reference_t(P &v, size_t index)
                : _v(v), _index(index) { }
        public:
            operator word_t<W>() const {
                return _v.get(_index);
            }
            
            template<typename = typename std::enable_if<not Const>::type>
            reference_t &operator=(word_t<W> value)
            {
                _v.set(_index, value);
                
                return *this;
            }
        };
        
    public:
        using value_type = word_t<W>;
        
        using reference = reference_t<false>;
        using const_reference = reference_t<true>;
        
        template<typename ...Args,
                 REQUIRES(std::is_constructible<Container, Args...>::value)>
        packed_view(size_t width, size_t size, Args&& ...args)
            : Container(std::forward<Args>(args)...), _width(width), _size(size)
        {
            assert(width > 0);
            assert(size > 0)
            assert(width <= W);
            assert(width * size <= W * length());
        }
        
        // Suggests the length of a buffer required to store the specified
        // number of elements of the specified width
        static size_t required_length(size_t width, size_t size)
        {
            return size_t(std::ceil((float(width) * size)/W));
        }
        
        // The number of presented elements
        size_t size() const { return _size; }
        
        // The length of the underlying container
        size_t length() const { return Container::size(); }
        
        word_t<W>      *data(size_t i)       { return &super()[0]; }
        word_t<W> const*data(size_t i) const { return &super()[0]; }
        
        const_reference operator[](size_t i) const {
            assert(i < size());
            return const_reference(*this, i);
        }
        
        reference operator[](size_t i) {
            assert(i < size());
            return reference(*this, i);
        }
        
    private:
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
            
            word_t<W> low  = get_bitfield(super()[i], l, l + llen);
            word_t<W> high = get_bitfield(super()[i + 1], 0, hlen) << llen;
            
            return high | low;
        }
        
        void set(size_t index, word_t<W> value)
        {
            assert(index < _size);
            
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(index);
            
            set_bitfield(&super()[i], l, l + llen, value);
            set_bitfield(&super()[i + 1], 0, hlen, value >> llen);
        }

    private:
        size_t _width; // Number of bits per element
        size_t _size; // Number of elements
    };
}
#endif


