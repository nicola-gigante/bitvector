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

#ifndef BITVECTOR_H
#define BITVECTOR_H

#include "bits.h"
#include "packed_view.h"

#include <cstddef>
#include <cmath>
#include <memory>

namespace bitvector
{
    template<size_t W>
    class bitvector
    {
        class node_vector
        {
            
        };
        
    public:
        bitvector(size_t capacity) : _capacity(capacity)
        {
            using std::floor;
            using std::ceil;
            using std::log2;
            
            _counter_size = ceil(log2(size_t(_capacity))) + 1;
            _degree = W / _counter_size;
            
            assert(_counter_size * _degree <= W);
        }
        
        size_t degree() const { return _degree; }
        size_t counter_size() const { return _counter_size; }
        size_t capacity() const { return _capacity; }
        
    private:
        size_t _degree = 0; /// Number of keys in each node
        size_t _height = 0; /// Height of the root node
        size_t _counter_size = 0; /// Size of the 'size' counter
        size_t _capacity = 0; /// Maximum number of bits stored in the vector
    };
}

#endif // BITVECTOR_H
