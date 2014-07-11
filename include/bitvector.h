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
#include <stdexcept>
#include <vector>

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
        class subtree_ref_t;
        
        template<bool Const>
        friend class subtree_ref_t;
        
        using subtree_ref       = subtree_ref_t<false>;
        using subtree_const_ref = subtree_ref_t<true>;
        
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
            
            // Compute masks for bit operations
            for(size_t i = 0; i < _degree; i++) {
                _index_mask <<= _counter_width;
                _index_mask |= 1;
            }
            
            // Allocate space for nodes and leaves
            _sizes.resize(_counter_width, nodes_count * _degree);
            _ranks.resize(_counter_width, nodes_count * _degree);
            _pointers.resize(_counter_width, nodes_count * (_degree + 1));
            
            _leaves.reserve(leaves_count);
            
            // Setup the initial root node
            _height = 1;
            root().pointers(0) = alloc_leaf();
        }
        
        void info() const {
            std::cout << "Word width = " << W << " bits\n"
                      << "Capacity = " << _capacity << " bits\n"
                      << "Counter width = " << _counter_width << " bits\n"
                      << "Degree = " << _degree << "\n"
                      << "b = " << _leaves_buffer << "\n"
                      << "b' = " << _nodes_buffer << "\n"
                      << "Number of nodes = " << _sizes.size() / _degree << "\n"
                      << "Number of leaves = " << _leaves.capacity() << "\n"
                      << "Index mask:\n"
                      << to_binary(_index_mask, _counter_width) << "\n"
                      << "Size flag mask:\n"
                      << to_binary(size_flag_mask(), _counter_width) << "\n";
        }
        
        size_t capacity() const { return _capacity; }
        size_t size() const { return _size; }
        size_t degree() const { return _degree; }
        
        size_t counter_width() const { return _counter_width; }
        
        bool empty() const { return _size == 0; }
        bool full() const { return _size == _capacity; }
        
        bool access(size_t index) const { return access(root(), index); }
        
        void insert(size_t index, bool bit) { insert(root(), index, bit); }
        
        subtree_ref       root()       { return { *this, _root, _height, _size }; }
        subtree_const_ref root() const { return { *this, _root, _height, _size }; }
        
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
        
        bool access(subtree_const_ref t, size_t index) const
        {
            if(index >= t.size())
                throw std::out_of_range("Index out of bounds");
            
            if(t.is_leaf()) // We have a leaf
                return get_bit(t.leaf(), index);
            else { // We're in a node
                size_t child, new_index;
                std::tie(child, new_index) = t.find(index);
                
                return access(t.child(child), new_index);
            }
        }
        
        void insert(subtree_ref t, size_t index, bool bit)
        {
            size_t child, new_index;
            std::tie(child, new_index) = t.find_insert_point(index);
            
            if(t.height() == 1) // We'll reach a leaf
            {
                // 1. Check if we need a split and/or a redistribution
                if(t.child(child).size() == W)
                {
                    // The leaf is full, we need a redistribution
                    size_t begin, end, count;
                    std::tie(begin, end, count) = find_adjacent_leaves(t, child);
                    
                    // Check if we need to split or only to redistribute
                    if(count >= _leaves_buffer * (W - _leaves_buffer)) {
                        // split...
                        assert(false && "Unimplemented");
                    }
                    
                    // redistribute
                    redistribute_bits(t, begin, end, count);
                    
                    // Search again where to insert the bit
                    std::tie(child, new_index) = t.find_insert_point(index);
                }
                
                // 2. Update counters
                t.sizes(child, degree()) += index_mask();
                t.ranks(child, degree()) += bit;
                
                // 3. Insert the bit
                word_t<W> &leaf = t.child(child).leaf();
                leaf = insert_bit(leaf, new_index, bit);
                _size += 1;
            }
            else // We'll have another node
            {
                // 1. Check if we need a split
                // FIXME: we have to look at the child node where we're going,
                //        not the current one
                // FIXME: Check nkeys
                size_t nkeys = std::get<0>(t.find_insert_point(t.size() - 1));
                if(nkeys == degree()) // We need a split
                {
                    // split()...
                    assert(false && "Unimplemented");
                }
                
                // 2. Update counters
                t.sizes(child, degree()) += index_mask();
                t.ranks(child, degree()) += bit;
                
                // 3. Continue the traversal
                insert(t.child(child), new_index, bit);
            }
        }
        
        // Utility functions for insert()
        
        // Find the group of leaves adjacent to 'child',
        // with the maximum number of free bits.
        // It returns a tuple with:
        //
        // - The begin and the end of the interval of the leaves
        // - The count of bits contained in total in the found leaves
        //   I repeat: the number of bits, not the number of free bits
        std::tuple<size_t, size_t, size_t>
        find_adjacent_leaves(subtree_ref t, size_t child)
        {
            size_t begin = child > _leaves_buffer ? child - _leaves_buffer - 1 : 0;
            size_t end = std::min(begin + _leaves_buffer, degree() + 1);
            
            size_t freebits = 0;
            size_t maxfreebits = 0;
            std::pair<size_t, size_t> window = { begin, end };
            
            // Sum for the initial window
            for(size_t i = begin; i < end; i++)
                freebits += W - t.child(i).size();
            
            maxfreebits = freebits;
            
            while(begin < child && end < degree() + 1)
            {
                freebits = freebits - (W - t.child(begin).size())
                                    + (W - t.child(end - 1).size());
                
                begin++;
                end++;
                
                if(freebits > maxfreebits) {
                    window = { begin, end };
                    maxfreebits = freebits;
                }
            }
            
            // Reverse the count of free bits to get the total number of bits
            size_t count = W * _leaves_buffer - maxfreebits;
            
            assert(begin <= child && child < end);
            return { window.first, window.second, count };
        }
        
        void redistribute_bits(subtree_ref t, size_t begin, size_t end, size_t count)
        {
            size_t b = end - begin;
            size_t bits_per_leaf = ceil(float(count) / b);
            
            assert(b == _leaves_buffer || b == _leaves_buffer + 1);
            
            std::vector<word_t<W>> leaves_bits(_leaves_buffer);
            packed_view<W> view(1, count, leaves_bits);
            
            for(size_t i = begin, p = 0; i < end; p += t.child(i).size(), i++)
                view(p, p + t.child(i).size()) = t.child(i).leaf();
            
            size_t p = 0;
            for(size_t i = begin; i < end; i++)
            {
                size_t n = std::min(count, bits_per_leaf);
                
                t.child(i).leaf() = view(p, p + n);
                if(i < degree())
                    t.sizes(i) = p + n;
                
                count -= n;
                p += n;
            }
            
            assert(count == 0);
        }
        
    private:
        // Maximum number of bits stored in the vector
        // Refered as N in the paper
        size_t _capacity;
        
        // Current number of bits stored in the bitvector
        size_t _size = 0;
        
        // Index of the root node of the tree
        size_t _root;
        
        // Height of the tree (distance of the root node from the leaves)
        size_t _height;
        
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
    class bitvector<W>::subtree_ref_t
    {
        using bitvector_t = typename
                      std::conditional<Const, bitvector const, bitvector>::type;
        
        using leaf_reference = typename
                          std::conditional<Const, word_t<W>, word_t<W>&>::type;
        
        using reference = typename
        std::conditional<Const, typename packed_array<W>::const_reference,
                                typename packed_array<W>::reference>::type;
        
        using size_reference = typename
        std::conditional<Const, typename packed_array<W, flag_bit>::const_reference,
                                typename packed_array<W, flag_bit>::reference>::type;
        
        // Reference to the parent bitvector structure
        bitvector_t &_vector;
        
        // Index of the root node of the subtree
        size_t _index = 0;
        
        // Height of the subtree (distance of the root from leaves)
        size_t _height = 0;
        
        // Total size (number of bits) of the subtree
        size_t _size = 0;
        
    public:
        subtree_ref_t(bitvector_t &vector, size_t index, size_t height, size_t size)
            : _vector(vector), _index(index), _height(height), _size(size) { }
        
        subtree_ref_t(subtree_ref_t const&) = default;
        subtree_ref_t &operator=(subtree_ref_t const&) = default;
        
        template<bool C, REQUIRES(Const && not C)>
        subtree_ref_t(subtree_ref_t<C> const&r)
            : subtree_ref_t(r._vector, r._index, r._height, r._size) { }
        
        // Height of the subtree
        size_t height() const { return _height; }
        
        // Pair of methods to know if the subtree is a leaf or not
        bool is_leaf() const { return _height == 0; }
        bool is_node() const { return _height > 0; }
        
        // Size of the subtree
        size_t size() const { return _size; }
        
        // Convenience shorthand for the degree of the subvector
        size_t degree() const { return _vector.degree(); }
        
        // This method creates a subtree_ref for the child at index k,
        // computing its size and its height. It's valid only if the height
        // is greater than 1 (so our children are internal nodes, not leaves).
        // For accessing the leaves of level 1 nodes, use the leaf() method
        subtree_ref_t child(size_t k) const
        {
            return child(k, std::integral_constant<bool, Const>());
        }
        
    private:
        // Helper functions for the child() method
        subtree_ref_t child(size_t k, std::true_type /* const */) const
        {
            assert(k <= degree());
            assert(pointers(k) != 0);
            
            return { _vector, pointers(k), height() - 1, subtree_size(k) };
        }
        
        // The non-const version allocates the node or the leaf if the
        // pointer at index k was null
        subtree_ref_t child(size_t k, std::false_type /* not const*/) const
        {
            assert(k <= degree());
            
            if(pointers(k) == 0)
                pointers(k) = height() == 1 ? _vector.alloc_leaf()
                                            : _vector.alloc_node();
            
            return child(k, std::true_type());
        }
        
        size_t subtree_size(size_t k) const
        {
            return k == 0        ? sizes(k) :
            k == degree() ? size() - sizes(k - 1) :
            sizes(k) - sizes(k - 1);
        }
        
    public:
        // Access to the value of the leaf, if this subtree_ref refers to a leaf
        leaf_reference leaf() const
        {
            assert(is_leaf());
            return _vector._leaves[_index];
        }
        
        // Finds the subtree where the bit at the given index can be inserted.
        // The position found by this function is suitable for insertion,
        // if you need lookup, use find().
        //
        // The returned pair contains:
        //  - The index of the subtree
        //  - The new index, relative to the subtree, where to insert the bit
        std::pair<size_t, size_t>
        find_insert_point(size_t index) const
        {
            assert(is_node());
            
            size_t child = degree() -
            popcount(_vector.size_flag_mask() &
                     (sizes() - _vector.index_mask() * word_t<W>(index)));
            
            size_t new_index = index - (child == 0 ? 0 : sizes(child - 1));
            
            //assert(new_index < this->child(child).size());
            
            return { child, new_index };
        }
        
        // Finds the subtree where the bit at the given index is located.
        //
        // The returned pair contains:
        //  - The index of the subtree
        //  - The new index, relative to the subtree, where to find the bit
        std::pair<size_t, size_t>
        find(size_t index) const
        {
            size_t child, new_index;
            std::tie(child, new_index) = find_insert_point(index);
            
            if(new_index == this->child(child).size()) {
                child += 1;
                new_index = 0;
            }
            
            assert(child < degree() + 1);
            
            return { child, new_index };
        }
        
        // Word composed by the size fields in the interval [begin, end)
        size_reference sizes(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin < degree());
            assert(end <= degree());
            return _vector._sizes(_index * degree() + begin, _index * degree() + end);
        }
        
        // Word of the size fields.
        size_reference sizes() const { return sizes(0, degree()); }
        
        // Value of the size field at index k (with the flag bit stripped)
        //
        // NOTE: This is NOT the size of the subtree rooted at index k.
        //       To get that, use n.child(k).size()
        //
        size_reference sizes(size_t k) const { return sizes(k, k + 1); }
        
        // Word composed by the rank fields in the interval [begin, end)
        reference ranks(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin < degree());
            assert(end <= degree());
            return _vector._ranks(_index * degree() + begin, _index * degree() + end);
        }
        
        // Word of the rank fields
        reference ranks() const { return ranks(0, degree()); }
        
        // Value of the rank field at index k
        reference ranks(size_t k) const { return ranks(k, k + 1); }
        
        // Word composed by the pointer fields in the interval [begin, end)
        reference pointers(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin == end || begin < degree() + 1);
            assert(end <= degree() + 1);
            return _vector._pointers(_index * (degree() + 1) + begin,
                                 _index * (degree() + 1) + end);
        }
        
        // Word of the pointer fields
        reference pointers() const { return pointers(0, degree() + 1); }
        
        // Value of the pointer field at index k
        reference pointers(size_t k) const { return pointers(k, k + 1); }
    };
}

#endif // BITVECTOR_H
