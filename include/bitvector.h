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
        template<bool Const>
        class node_ptr_t;
        
        template<bool>
        friend class node_ptr_t;
        
        using node_ptr       = node_ptr_t<false>;
        using const_node_ptr = node_ptr_t<true>;
        
        using leaf_ptr       = word_t<W> *;
        using const_leaf_ptr = word_t<W> const*;
        
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
            
            // Allocate space for nodes and leaves
            _sizes.resize(_counter_width, nodes_count * _degree);
            _ranks.resize(_counter_width, nodes_count * _degree);
            _pointers.resize(_counter_width, nodes_count * (_degree + 1));
            
            _leaves.reserve(leaves_count);
            
            // Setup the initial root node
            _root = node_ptr(*this, alloc_node());
            _root.pointer(0) = alloc_leaf();
            _height = 1;
            
            // Compute masks
            for(size_t i = 0; i < _degree; i++) {
                _index_mask <<= _counter_width;
                _index_mask |= 1;
            }
        }
        
        void info() const {
            std::cout << "Word width = " << W << " bits\n"
                      << "Capacity = " << _capacity << " bits\n"
                      << "Counter width = " << _counter_width << " bits\n"
                      << "Degree = " << _degree << "\n"
                      << "b = " << _leaves_buffer << "\n"
                      << "b' = " << _nodes_buffer << "\n"
                      << "Number of nodes = " << _sizes.size() / _degree << "\n"
                      << "Number of leaves = " << _leaves.size() << "\n"
                      << "Index mask:\n"
                      << to_binary(_index_mask, _counter_width) << "\n"
                      << "Size flag mask:\n"
                      << to_binary(size_flag_mask(), _counter_width) << "\n";
        }
        
        size_t capacity() const { return _capacity; }
        size_t size() const { return _size; }
        size_t degree() const { return _degree; }
        
        size_t counter_width() const { return _counter_width; }
        
        node_ptr root() const { return _root; }
        
        bool empty() const { return _size == 0; }
        bool full() const { return _size == _capacity; }
        
        bool access(size_t index)
        {
            return access(index, _root, _height, _size);
        }
        
        void insert(size_t index, bool bit)
        {
            insert(index, bit, _root, _height, _size);
        }
        
    private:
        // This "allocation" is only to take the first free node and return it
        size_t alloc_node() {
            assert(_free_node < _sizes.size());
            return _free_node++;
        }
        
        size_t alloc_leaf() {
            _leaves.push_back(0);
            return _leaves.size() - 1;
        }
        
        word_t<W> index_mask() const { return _index_mask; }
        
        word_t<W> size_flag_mask() const {
            return _index_mask << (_counter_width - 1);
        }
        
        std::tuple<size_t, size_t, size_t>
        locate_subtree(size_t index, node_ptr n, size_t total_size)
        {
            size_t child = degree() -
                           popcount(size_flag_mask() &
                                    (n.sizes() - index_mask() * index));
            
            size_t new_size = child == 0        ? n.size(child) :
                              child == degree() ? total_size :
                                                  n.size(child) - n.size(child - 1);
            
            size_t new_index = index - (child == 0 ? 0 : n.size(child - 1));
            
            assert(new_index < new_size);
            
            return { child, new_size, new_index };
        }
        
        bool access(size_t index, node_ptr n, size_t h, size_t size)
        {
            assert(h > 0);
            
            size_t child, new_size, new_index;
            std::tie(child, new_size, new_index) = locate_subtree(index, n, size);
            
            if(h == 1) // We'll reach a leaf
                return get_bit(n.leaf(child), new_index);
            else // We'll have another node
                return access(new_index, n.child(child), h - 1, new_size);
        }
        
        void insert(size_t index, bool bit, node_ptr n, size_t h, size_t size)
        {
            assert(h > 0);
            
            size_t child, new_size, new_index;
            std::tie(child, new_size, new_index) = locate_subtree(index, n, size);
            
            if(h == 1) // We'll reach a leaf
            {
                // 1. Check if we need a split
                if(n.size(child) == W) // We need a split
                {
                    // split()...
                }
                
                // 2. Insert the bit
                size_t &leaf = n.leaf(child);
                leaf = insert_bit(leaf, new_index, bit);
            }
            else // We'll have another node
            {
                // 1. Check if we need a split
                size_t nkeys = locate_subtree(size - 1, n, size);
                if(nkeys == degree())
                {
                    // split()...
                }
                
                // 2. Update counters
                n.sizes(child, degree()) += 1;
                n.ranks(child, degree()) += bit;
                
                // 3. Continue the traversal
                insert(new_index, bit, n.child(child), h - 1, new_size);
            }
        }
        
    private:
        // Maximum number of bits stored in the vector
        // Refered as N in the paper
        size_t _capacity;
        
        // Current number of bits stored in the bitvector
        size_t _size = 0;
        
        // Pointer to the root node of the tree
        node_ptr _root;
        
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
        
        // Current height of the root tree
        size_t _height;
        
        // Index of the first unused node in the nodes arrays
        size_t _free_node = 0;
        
        // Mask with the LSB bit set each |size| bits, used in find_child()
        word_t<W> _index_mask = 0;
        
        // Packed arrays of data representing the nodes
        packed_array<W, flag_bit> _sizes;
        packed_array<W> _ranks;
        packed_array<W> _pointers;
        std::vector<word_t<W>> _leaves;
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
        
        using size_reference = typename
        std::conditional<Const, typename packed_array<W, flag_bit>::const_reference,
                                typename packed_array<W, flag_bit>::reference>::type;
        
        BV *_v = nullptr;
        size_t _index = 0;
        
    public:
        node_ptr_t() = default;
        node_ptr_t(std::nullptr_t) { }
        
        node_ptr_t(BV &v, size_t index)
            : _v(&v), _index(index) { }
        
        node_ptr_t(node_ptr_t const&) = default;
        node_ptr_t &operator=(node_ptr_t const&) = default;
        
        bool null() const { return _v == nullptr; }
        
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
        
        size_reference sizes(size_t begin, size_t end) const
        {
            assert(!null());
            assert(begin < degree());
            assert(end <= degree());
            return _v->_sizes(_index * degree() + begin, _index * degree() + end);
        }
        
        size_reference sizes() const
        {
            assert(!null());
            return sizes(0, degree());
        }
        
        size_reference size(size_t k) const
        {
            assert(!null());
            assert(k < degree());
            return _v->_sizes[_index * degree() + k];
        }
        
        reference rank(size_t k) const
        {
            assert(!null());
            assert(k < degree());
            return _v->_ranks[_index * degree() + k];
        }
        
        reference ranks(size_t begin, size_t end) const
        {
            assert(!null());
            assert(begin < degree());
            assert(end <= degree());
            return _v->_ranks(_index * degree() + begin, _index * degree() + end);
        }
        
        reference ranks() const
        {
            assert(!null());
            return ranks(0, degree());
        }
        
        reference pointer(size_t k) const
        {
            assert(!null());
            assert(k <= degree());
            return _v->_pointers[_index * (degree() + 1) + k];
        }
        
        reference pointers(size_t begin, size_t end) const
        {
            assert(!null());
            assert(begin < degree());
            assert(end <= degree() + 1);
            return _v->_pointers(_index * (degree() + 1) + begin,
                                 _index * (degree() + 1) + end);
        }
        
        reference pointers() const
        {
            assert(!null());
            return pointers(0, degree() + 1);
        }
        
        node_ptr_t child(size_t k) const
        {
            assert(!null());
            assert(k <= degree());
            
            return { *_v, pointer(k) };
        }
        
        word_t<W> leaf(size_t k) const
        {
            assert(!null());
            return _v->_leaves[size_t(pointer(k))];
        }
        
        word_t<W> &leaf(size_t k)
        {
            assert(!null());
            return _v->_leaves[size_t(pointer(k))];
        }
    };
}

#endif // BITVECTOR_H
