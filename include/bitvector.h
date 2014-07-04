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
    using std::floor;
    using std::ceil;
    using std::log2;
    using std::sqrt;
    using std::pow;
    
    template<size_t W>
    class bitvector
    {
        class nodes_t {
            packed_vector<W> _sizes;
            packed_vector<W> _ranks;
            packed_vector<W> _pointers;
        public:
            nodes_t(size_t field_size, size_t degree, size_t size);
        };
        
    public:
        bitvector(size_t capacity) : _capacity(capacity)
        {
            _counter_size = ceil(log2(_capacity)) + 1;
            
            _degree = W / _counter_size;
            
            _leaves_buffer = ceil(sqrt(W) - 1);
            
            _nodes_buffer  = ceil(sqrt(_degree) - 1);
            
            // Total number of leaves to allocate space for
            size_t leaves_count = ceil(_capacity *
                                       ((_leaves_buffer * (W - _leaves_buffer)) /
                                        (_leaves_buffer + 1)));
            
            // Maximum height of the tree
            size_t max_height = ceil(log(leaves_count) / log(_degree + 1));
            
            // Minimum number of fields for each node
            size_t minimum_degree = (_nodes_buffer * (_degree - _nodes_buffer)) /
                                    (_nodes_buffer - 1);
            
            // Total number of internal nodes
            size_t nodes_count = ceil(leaves_count /
                                      pow(minimum_degree + 1, max_height));
        }
        
        size_t degree() const { return _degree; }
        size_t counter_size() const { return _counter_size; }
        size_t capacity() const { return _capacity; }
        
    private:
        packed_view<W, std::vector> _data;
        
        // Maximum number of bits stored in the vector
        // Refered as N in the paper
        size_t _capacity;
        
        // Size of the nodes' 'size' counter
        size_t _counter_size;

        // Number of counters per node, refered as d in the paper
        size_t _degree;
        
        // Number of leaves used for redistribution for ammortized
        // constant insertion time. Refered as b in the paper
        size_t _leaves_buffer;
        
        // Number of leaves used for redistribution for ammortized
        // constant insertion time. Refered as b' in the paper
        size_t _nodes_buffer;
        
        // Height of the root node (i.e. of the tree)
        size_t _height;
        
        // Container of the arrays of data representing the nodes
        nodes_t _nodes;
    };
}

#endif // BITVECTOR_H
