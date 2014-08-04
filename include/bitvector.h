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
#include <stdexcept>
#include <vector>
#include <atomic>

#include <ostream>
#include <iomanip>

// FIXME: remove this include after having finished the debugging
#include <iostream>

namespace bitvector
{
    struct bt_impl;
    class bitvector
    {
    public:
        /*
         * Constructors, copies and moves...
         */
        bitvector(size_t N, size_t W = 256);

        bitvector(bitvector const&);
        bitvector(bitvector &&) = default;
        
        bitvector &operator=(bitvector const&);
        bitvector &operator=(bitvector &&) = default;
        
        /*
         * Accessors
         */
        bool empty() const;
        bool full() const;
        
        // Debugging output
        friend std::ostream &operator<<(std::ostream &s, bitvector const&v);
        
    private:
        std::unique_ptr<bt_impl> _impl;
    };
    
#if 0
    class bt_impl
    {
    public:
        
        size_t leaves_minimum_size() const {
            return (_leaves_buffer * (_node_width - _leaves_buffer)) /
                   (_leaves_buffer + 1);
        }
        
        bool access(size_t index) const { return access(root(), index); }
        
        void insert(size_t index, bool bit) { insert(root(), index, bit); }
        
        subtree_ref       root()       { return { *this, 0, _height, _size, _rank }; }
        subtree_const_ref root() const { return { *this, 0, _height, _size, _rank }; }
        
    private:
        
        
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
            if(index > t.size())
                throw std::out_of_range("Index out of bounds");
            
            // If we see a full node in this point it must be the root,
            // otherwise we've violated our invariants.
            // So, since it's a root and it's full, we prepare the split
            // by allocating a new node and swapping it with the old root,
            // thus ensuring the root is always at index zero.
            // Then we go ahead pretending we started the insertion from the
            // new root, so we don't duplicate the node splitting code
            // below
            if(t.is_full())
            {
                assert(t.is_root());
                
                // Copy the old root into another node
                subtree_ref old_root = t.copy();
                
                // Empty the root and make it point to the old one
                t.sizes() = index_mask() * t.size();
                t.ranks() = index_mask() * t.rank();
                t.pointers() = 0;
                t.pointers(0) = old_root.index();
                
                // The only point in the algorithm were the height increases
                ++_height;
                
                assert(root().nchildren() == 1);
                assert(root().child(0).is_full());
                
                // Pretend we were inserting from the new root
                return insert(root(), index, bit);
            }
            
            // If we're here we assume the node is not full
            assert(!t.is_full());
            
            // Find where we have to insert this bit
            size_t child, new_index;
            std::tie(child, new_index) = t.find_insert_point(index);
            
            // Then go ahead
            if(t.height() == 1) // We'll reach a leaf
            {
                // 1. Check if we need a split and/or a redistribution of bits
                if(t.child(child).is_full())
                {
                    // The leaf is full, we need a redistribution
                    size_t begin, end, count;
                    std::tie(begin, end, count) = find_adjacent_children(t, child);
                    
                    // Check if we need to split or only to redistribute
                    if(count >= _leaves_buffer * (W - _leaves_buffer))
                        // We need to split. The node should not be full
                        t.insert_child(end++);
                    
                    // redistribute
                    redistribute_bits(t, begin, end, count);
                    
                    // It's important to stay in shape and not get to fat
                    for(size_t k = begin; k < end; ++k)
                        assert(_size < (_leaves_buffer * (W - _leaves_buffer)) ||
                               t.child(k).size() >= (_leaves_buffer * (W - _leaves_buffer)) /
                                                    (_leaves_buffer + 1));
                    
                    // Search again where to insert the bit
                    std::tie(child, new_index) = t.find_insert_point(index);
                }
                
                // 2. Update counters
                _size += 1;
                _rank += bit;
                t.sizes(child, degree()) += index_mask();
                t.ranks(child, degree()) += index_mask() * bit;
                
                // 3. Insert the bit
                word_t<W> &leaf = t.child(child).leaf();
                leaf = insert_bit(leaf, new_index, bit);
            }
            else // We'll have another node
            {
                // 1. Check if we need a split and/or a redistribution of keys
                if(t.child(child).is_full())
                {
                    // The node is full, we need a redistribution
                    size_t begin, end, count;
                    std::tie(begin, end, count) = find_adjacent_children(t, child);
                    
                    if(count / (_nodes_buffer + 1) >= _nodes_buffer)
                        t.insert_child(end++); // We need to split.
                    
                    // redistribute
                    redistribute_keys(t, begin, end, count);
                    
                    // Search again where to insert the bit
                    std::tie(child, new_index) = t.find_insert_point(index);
                }
                
                // Get the ref to the child into which we're going to recurse,
                // Note that we need to get the ref before incrementing the counters
                // in the parent, for consistency
                subtree_ref next_child = t.child(child);
                
                t.sizes(child, degree()) += index_mask();
                t.ranks(child, degree()) += index_mask() * bit;
                
                // 3. Continue the traversal
                insert(next_child, new_index, bit);
            }
        }
        
        // Utility functions for insert()
        
        // Find the group of children adjacent to 'child',
        // with the maximum number of free slots (bits or keys, it depends).
        // It returns a tuple with:
        //
        // - The begin and the end of the interval selected interval of children
        // - The count of slots contained in total in the found leaves
        //   I repeat: the number of slots, not the number of free slots
        std::tuple<size_t, size_t, size_t>
        find_adjacent_children(subtree_ref t, size_t child)
        {
            const bool is_leaf = t.child(child).is_leaf();
            const size_t buffer = is_leaf ? _leaves_buffer : _nodes_buffer;
            const size_t max_count = is_leaf ? W : (degree() + 1);
            const auto count = [&](size_t i) {
                return t.pointers(i) == 0 ? max_count :
                       is_leaf            ? W - t.child(i).size() :
                                            (degree() + 1) - t.child(i).nchildren();
            };
            
            size_t begin = child >= buffer ? child - buffer + 1 : 0;
            size_t end = std::min(begin + buffer, degree() + 1);
            
            size_t freeslots = 0;
            size_t maxfreeslots = 0;
            std::pair<size_t, size_t> window = { begin, end };
            
            // Sum for the initial window
            for(size_t i = begin; i < end; ++i)
                freeslots += count(i);
            maxfreeslots = freeslots;
            
            // Slide the window
            while(begin < child && end < degree() + 1)
            {
                freeslots = freeslots - count(begin) + count(end - 1);
                
                begin += 1;
                end += 1;
                
                if(freeslots > maxfreeslots) {
                    window = { begin, end };
                    maxfreeslots = freeslots;
                }
            }
            
            // Reverse the count of free slots to get the total number of bits
            size_t total = max_count * buffer - maxfreeslots;
            
            assert(window.first <= child && child < window.second);
            return { window.first, window.second, total };
        }
        
        //
        // This function clears the counters relative to children
        // in the range [begin, end), as if the respective subtrees were empty.
        // Pointers and subtrees are not touched.
        //
        void clear_children_counters(subtree_ref t, size_t begin, size_t end) const
        {
            size_t keys_end = std::min(end, degree());
            size_t last_size = end < degree() ? t.sizes(end - 1) : size();
            size_t last_rank = end < degree() ? t.ranks(end - 1) : rank();
            size_t prev_size = begin > 0 ? t.sizes(begin - 1) : 0;
            size_t prev_rank = begin > 0 ? t.ranks(begin - 1) : 0;
            
            t.sizes(begin, keys_end)     = index_mask() * prev_size;
            t.ranks(begin, keys_end)     = index_mask() * prev_rank;
            t.sizes(keys_end, degree()) -= index_mask() * last_size;
            t.ranks(keys_end, degree()) -= index_mask() * last_rank;
        }
        
        // FIXME: rewrite using a temporary space of two words instead
        //        of the whole buffer, thus avoiding the dynamic allocation
        void redistribute_bits(subtree_ref t, size_t begin, size_t end, size_t count)
        {
            size_t b = end - begin; // Number of children to use
            size_t bits_per_leaf = count / b; // Average number of bits per leaf
            size_t rem           = count % b; // Remainder
            
            assert(b == _leaves_buffer || b == _leaves_buffer + 1);
            
            // Here we use the existing abstraction of packed_array
            // to accumulate all the bits into a temporary buffer, and
            // subsequently redistribute them to the leaves
            packed_array<W> bits(1, count, b);
            
            for(size_t i = begin, p = 0; i < end; ++i) {
                if(t.pointers(i) != 0) {
                    bits(p, p + t.child(i).size()) = t.child(i).leaf();
                    p += t.child(i).size();
                }
            }
            
            clear_children_counters(t, begin, end);
            
            // The redistribution begins.
            for(size_t p = 0, i = begin; i < end; ++i)
            {
                // The remainder is evenly distributed between the first leaves
                size_t n = bits_per_leaf;
                if(rem) {
                    n += 1;
                    rem -= 1;
                }
                
                // Here we take into account the first steps, when
                // we have an empty root to fill up. If we're going to use
                // a children that doesn't exist, we create it.
                if(t.pointers(i) == 0)
                    t.insert_child(i);
                
                // Take the bits out of the buffer and count the rank
                // and put them back into the leaf
                word_t<W> leaf = bits(p, p + n);
                t.child(i).leaf() = leaf;
                
                // Increment the counters
                t.sizes(i, degree()) += index_mask() * n;
                t.ranks(i, degree()) += index_mask() * popcount(leaf);
                
                // Count of the copied bits
                p += n;
                count -= n;
            }
            
            assert(count == 0);
        }
        
        // FIXME: rewrite using a temporary space of two words instead
        //        of the whole buffer, thus avoiding the dynamic allocation
        void redistribute_keys(subtree_ref t, size_t begin, size_t end, size_t count)
        {
            size_t b = end - begin;
            size_t keys_per_node = count / b;
            size_t rem           = count % b;
            
            assert(b == _nodes_buffer || b == _nodes_buffer + 1);
            
            struct pointer {
                size_t size;
                size_t rank;
                size_t ptr;
            };
            
            std::vector<pointer> pointers;
            pointers.reserve(_nodes_buffer * (degree() + 1));
        
            for(size_t i = begin; i != end; ++i) {
                if(t.pointers(i) != 0) {
                    for(size_t c = 0; c < t.child(i).nchildren(); ++c) {
                        pointers.push_back({ t.child(i).child(c).size(),
                            t.child(i).child(c).rank(),
                            t.child(i).pointers(c) });
                    }
                }
            }
            
            clear_children_counters(t, begin, end);
            
            for(size_t p = 0, i = begin; i != end; ++i)
            {
                size_t n = keys_per_node;
                if(rem) {
                    n += 1;
                    rem -= 1;
                }
                
                if(t.pointers(i) == 0)
                    t.insert_child(i);
                
                // Clear the node
                t.child(i).sizes() = 0;
                t.child(i).ranks() = 0;
                t.child(i).pointers() = 0;
                
                size_t childsize = 0;
                size_t childrank = 0;
                for(size_t j = 0; j < n; ++j)
                {
                    size_t s = pointers[p + j].size;
                    size_t r = pointers[p + j].rank;
                    
                    t.child(i).pointers(j) = pointers[p + j].ptr;
                    t.child(i).sizes(j, degree()) += index_mask() * s;
                    t.child(i).ranks(j, degree()) += index_mask() * r;
                    
                    childsize += s;
                    childrank += r;
                }
                
                t.sizes(i, degree()) += index_mask() * childsize;
                t.ranks(i, degree()) += index_mask() * childrank;
                
                count -= n;
                p += n;
            }
            
            assert(count == 0);
        }
        
    private:
        
    };
    
    
#endif
}

#endif // BITVECTOR_H
