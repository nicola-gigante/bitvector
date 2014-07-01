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

namespace bitvector
{
    template<size_t W>
    class packed_vector
    {
    public:
        using value_type = size_t;
        
        class reference
        {
            friend class packed_vector;
            
            packed_vector &_v;
            size_t _index;
            
            reference(packed_vector &v, size_t index)
                : _v(v), _index(index) { }
        public:
        };
        
        packed_vector(size<bits> width, size_t size)
            : _width(width), _size(size)
        {
            assert(width <= W);
            
            size_t length = size_t(std::ceil((float(width) * size)/W));
            _data.resize(length);
        }
        
        size_t get(size_t index) const
        {
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(index);
            
            size_t low = _data[i] << l;
            size_t high = hlen != 0 ? _data[i + 1] >> (W - hlen) : 0;
            
            return low | high;
        }
        
        void set(size_t index, size_t value)
        {
            size_t i, l, llen, hlen;
            std::tie(i, l, llen, hlen) = locate(index);
            
            // set the value...
        }
        
    private:
        std::tuple<size_t, size_t, size_t, size_t>
        locate(size_t index) const {
            size_t i = (_width * index) / W; // Index of the word
            size_t l = (_width * index) % W; // Bit index inside the word
            size_t llen = std::min(W - l, _width); // length of the low part
            size_t hlen = _width - llen; // Length of the high part
            
            return { i, l, llen, hlen };
        }
        
        std::vector<word_t<W>> _data;
        
        size<bits> _width; // Number of bits per element
        size_t _size; // Number of elements
    };
}
#endif
