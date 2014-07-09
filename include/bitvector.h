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
#include <type_traits>
#include <bitset>

#include <iostream>
#include <iomanip>

namespace bitvector
{
    using std::floor;
    using std::ceil;
    using std::log2;
    using std::sqrt;
    using std::pow;
    using std::min;
    using std::max;
    
    template<size_t W>
    class bitvector
    {
    public:
        bitvector(size_t capacity)
        {
            _capacity = capacity;
            
            _counter_width = ceil(log2(_capacity)) + 1;
            
            _degree = W / _counter_width;
            
            _leaves_buffer = min(max(size_t(ceil(sqrt(W)) - 1), size_t(1)),
                                 _degree);
            
            _nodes_buffer  = max(size_t(ceil(sqrt(_degree)) - 1), size_t(1));
            
            // Total number of leaves to allocate space for
            size_t leaves_count = ceil(_capacity * (_leaves_buffer + 1) /
                                      (_leaves_buffer * (W - _leaves_buffer)));
            
            // Minimum number of fields for each node
            size_t minimum_degree = (_nodes_buffer * (_degree - _nodes_buffer)) /
                                    (_nodes_buffer + 1);
            
            // Total number of internal nodes
            size_t nodes_count = 0;
            size_t level_count = leaves_count;
            do
            {
                level_count = ceil(float(level_count) / (minimum_degree + 1));
                nodes_count += level_count;
            } while(level_count > 1);
            
            _sizes.resize(_counter_width, nodes_count * _degree);
            _ranks.resize(_counter_width, nodes_count * _degree);
            _pointers.resize(_counter_width, nodes_count * (_degree + 1));
            
            _leaves.reserve(leaves_count);
            
            _root = node_ptr(*this, alloc_node());
            _root.pointer(0) = alloc_leaf();
            _height = 1;
        }
        
        void info() const {
            std::cout << "Word width = " << W << " bits\n"
                      << "Capacity = " << _capacity << " bits\n"
                      << "Counter width = " << _counter_width << " bits\n"
                      << "Degree = " << _degree << "\n"
                      << "b = " << _leaves_buffer << "\n"
                      << "b' = " << _nodes_buffer << "\n"
                      << "Number of nodes = " << _sizes.size() / _degree << "\n"
                      << "Number of leaves = " << _leaves.size() << "\n";
        }
        
        size_t capacity() const { return _capacity; }
        size_t size() const { return _size; }
        size_t degree() const { return _degree; }
        
        bool empty() const { return _size == 0; }
        bool full() const { return _size == _capacity; }
        
        bool access(size_t index)
        {
            return access(index, _root, _height);
        }
        
    private:
        template<bool Const>
        class node_ptr_t;
        
        using node_ptr       = node_ptr_t<false>;
        using const_node_ptr = node_ptr_t<true>;
        
        using leaf_ptr       = word_t<W> *;
        using const_leaf_ptr = word_t<W> const*;
        
        // This "allocation" is only to take the first free node and return it
        size_t alloc_node() {
            assert(_free_node < _sizes.size());
            return _free_node++;
        }
        
        size_t alloc_leaf() {
            _leaves.push_back(0);
            return _leaves.size() - 1;
        }
        
        bool access(size_t index, node_ptr n, size_t h)
        {
            assert(h > 0);
            return false;
        }
        
    private:
        // Maximum number of bits stored in the vector
        // Refered as N in the paper
        size_t _capacity;
        
        // Current number of bits stored in the bitvector
        size_t _size = 0;
        
        // Pointer to the root node of the tree
        node_ptr _root;
        
        // Current height of the root tree
        size_t _height;
        
        // Index of the first unused node in the nodes arrays
        size_t _free_node = 0;
        
        // Packed arrays of data representing the nodes
        packed_array<W, flag_bit> _sizes;
        packed_array<W> _ranks;
        packed_array<W> _pointers;
        std::vector<word_t<W>> _leaves;
        
        // Bit width of the nodes' counters inside nodes' words
        size_t _counter_width;
        
        // Number of counters per node, refered as d in the paper
        size_t _degree;
        
        // Number of leaves used for redistribution for ammortized
        // constant insertion time. Refered as b in the paper
        size_t _leaves_buffer;
        
        // Number of leaves used for redistribution for ammortized
        // constant insertion time. Refered as b' in the paper
        size_t _nodes_buffer;
    };
    
    template<size_t W>
    template<bool Const>
    class bitvector<W>::node_ptr_t
    {
        using BV = typename
        std::conditional<Const, const bitvector<W>, bitvector<W>>::type;
        
        using reference = typename
        std::conditional<Const, typename packed_array<W>::const_reference,
                                typename packed_array<W>::reference>::type;
        
        BV *_v = nullptr;
        size_t _index = 0;
        
    public:
        node_ptr_t() = default;
        node_ptr_t(std::nullptr_t) { }
        
        node_ptr_t(BV &v, size_t index)
            : _v(&v), _index(index) { }
        
        node_ptr_t(node_ptr_t const&) = default;
        node_ptr_t &operator=(node_ptr_t const&) = default;
        
        bool operator!() const
        {
            return _v == nullptr;
        }
        
        explicit operator bool() const
        {
            return _v != nullptr;
        }
        
        operator size_t() const {
            return _index;
        }
        
        size_t degree() const {
            return _v->degree();
        }
        
        reference sizes() const
        {
            return _v->_sizes(_index * degree(), _index * degree() + degree());
        }
        
        reference size(size_t k) const
        {
            assert(k < degree());
            return _v->_sizes[_index * degree() + k];
        }
        
        reference rank(size_t k) const
        {
            assert(k < degree());
            return _v->_ranks[_index * degree() + k];
        }
        
        reference ranks() const
        {
            return _v->_ranks(_index * degree(), _index * degree() + degree());
        }
        
        reference pointer(size_t k) const
        {
            assert(k <= degree());
            return _v->_pointers[_index * (degree() + 1) + k];
        }
        
        reference pointers() const
        {
            return _v->_pointers(_index * (degree() + 1),
                                 _index * (degree() + 1) + degree() + 1);
        }
        
        const_node_ptr child(size_t k) const
        {
            assert(k <= degree());
            
            return { _v, pointer(k) };
        }
        
        word_t<W> leaf(size_t k) const
        {
            return _v->_leaves[size_t(pointer(k))];
        }
        
        word_t<W> &leaf(size_t k)
        {
            return _v->_leaves[size_t(pointer(k))];
        }
        
        // The operator-> could be useless, but since this tries to be a smart
        // pointer, it does support pointer syntax.
        // But it's a lie. In this way, node->member is the same as node.member
        node_ptr_t *operator->() {
            return this;
        }
        
        node_ptr_t const*operator->() const {
            return this;
        }
    };
}

#endif // BITVECTOR_H
