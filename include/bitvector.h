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

#include "utility.h"

#include <cstddef>
#include <cmath>
#include <memory>

namespace bitvector
{
    template<size_t W>
    class bitvector
    {
    public:
        static_assert(W >= sizeof(void*) * 8,
                      "Word size must be at least the size of a pointer");
        
        using word = word_t<W>;
        constexpr static size<bits> word_size = W;
        
        bitvector(size<bits> capacity) : _capacity(capacity)
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
        
    //private:
        struct node
        {
            word sizes = 0;
            word ranks = 0;
        };
        
        node *create_node(size_t depth)
        {
            const size_t ptrlength = (depth == _heigth ? sizeof(word)
                                                       : sizeof(node *));
            const size_t length = sizeof(node) + (_degree + 1) * ptrlength;
            node *n = reinterpret_cast<node *>(new char[length] { });
            
            return n;
        }
        
    private:
        size_t _degree = 0;
        size_t _heigth = 0; /// Depth of leaves
        size<bits> _counter_size = 0;
        size<bits> _capacity = 0;
    };
}

#endif // BITVECTOR_H
