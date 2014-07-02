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

#ifndef BITVECTOR_PACKED_ARRAY_H
#define BITVECTOR_PACKED_ARRAY_H

#include "bits.h"

#include <vector>
#include <type_traits>

namespace bitvector
{
    template<size_t W>
    class packed_array
    {
        template<bool Const>
        class reference_t
        {
            friend class packed_array;
            
            using P = typename std::conditional<Const,
            packed_array const,
            packed_array>::type;
            
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
        
        packed_array(size<bits> width, size_t size)
            : _width(width), _size(size)
        {
            assert(width <= W);
            
            size_t length = size_t(std::ceil((float(width) * size)/W));
            _data.resize(length);
        }
        
        size_t size() const { return _size; }
        
        word_t<W> &word(size_t i) { return _data[i]; }
        word_t<W> word(size_t i) const { return _data[i]; }
        
        const_reference operator[](size_t i) const {
            return const_reference(*this, i);
        }
        
        reference operator[](size_t i) {
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
            
            word_t<W> low  = get_bitfield(_data[i], l, l + llen);
            word_t<W> high = get_bitfield(_data[i + 1], 0, hlen) << llen;
            
            return high | low;
        }
        
        void set(size_t index, word_t<W> value)
        {
            assert(index < _size);
            
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(index);
            
            set_bitfield(&_data[i], l, l + llen, value);
            set_bitfield(&_data[i + 1], 0, hlen, value >> llen);
        }
        
    private:
        std::vector<word_t<W>> _data;
        
        ::bitvector::size<bits> _width; // Number of bits per element
        size_t _size; // Number of elements
    };
}
#endif
