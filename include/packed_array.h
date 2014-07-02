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
        
    private:
        std::vector<word_t<W>> _data;
        
        size<bits> _width; // Number of bits per element
        size_t _size; // Number of elements
    };
}
#endif
