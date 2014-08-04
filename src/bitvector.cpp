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

#include "bitvector.h"

#include "bits.h"
#include "packed_view.h"

#include <cstddef>

namespace bitvector
{
    using std::floor;
    using std::ceil;
    using std::log2;
    using std::sqrt;
    using std::pow;
    using std::min;
    using std::max;
    
    /*
     * Private implementation class for bitvector
     */
    struct bt_impl
    {
        /*
         * Types
         */
        template<bool Const>
        class subtree_ref_base;
        
        class subtree_ref;
        using subtree_const_ref = subtree_ref_base<true>;
        
        using leaf_t = uint64_t;
        
        using packed_data = packed_view<std::vector>;
        using field_type = packed_data::value_type;
        
        /*
         * Public relations
         */
        template<bool Const>
        friend class subtree_ref_base;
        friend class subtree_ref;
        
        /*
         * Data
         */
        // Maximum number of bits stored in the vector
        // Refered as N in the paper
        size_t capacity;
        
        // Number of bits used for a node
        size_t node_width;
        
        // Current number of bits stored in the bitvector
        size_t size = 0;
        
        // Total rank of the bitvector
        size_t rank = 0;
        
        // Height of the tree (distance of the root node from the leaves)
        size_t height = 1;
        
        // Bit width of the nodes' counters inside nodes' words
        size_t counter_width;
        
        // Bit width of nodes' pointers
        size_t pointer_width;
        
        // Number of counters per node, refered as d in the paper
        size_t degree;
        
        // Number of leaves used for redistribution for ammortized
        // constant insertion time. Refered as b in the paper
        size_t leaves_buffer;
        
        // Number of leaves used for redistribution for ammortized
        // constant insertion time. Refered as b' in the paper
        size_t nodes_buffer;
        
        // Index of the first unused node in the nodes arrays
        size_t free_node = 0;
        
        // Index of the first unused leaf in the leaves array
        // Starts from 1 because of the unused sentinel for null pointers to
        // leaves (not needed for internal nodes)
        size_t free_leaf = 1;
        
        // Packed arrays of data representing the nodes
        packed_data sizes;
        packed_data ranks;
        packed_data pointers;
        std::vector<leaf_t> leaves;
        
        /*
         * Operations
         */
        // Default copy operations work well
        bt_impl(bt_impl const&) = default;
        bt_impl &operator=(bt_impl const&) = default;
        
        // Parameters initialization at construction
        bt_impl(size_t capacity, size_t node_width);
        
        // We don't want trivial accessors in the private interface, but these
        // two do something different.
        size_t used_leaves() const { return free_leaf - 1; }
        size_t used_nodes() const { return free_node; }
        
        // Nodes and leaves allocation
        size_t alloc_node();
        size_t alloc_leaf();
    };
    
    /*
     * Parameters are computed according to the required maximum capacity.
     * Please refer to the paper for a detailed explaination.
     */
    bt_impl::bt_impl(size_t N, size_t W)
    {
        capacity = N;
        node_width = W;
        
        counter_width = ceil(log2(capacity)) + 1;
        
        degree = node_width / counter_width;
        
        for(nodes_buffer = max(size_t(ceil(sqrt(degree))), size_t(1));
            floor((degree + 1)/nodes_buffer) < nodes_buffer;
            --nodes_buffer);
        
        // b and b' were different parameters before. Let them still
        // be different variables, just in case...
        leaves_buffer = nodes_buffer;
        
        // Total number of leaves to allocate space for
        size_t leaves_count = ceil(capacity /
                                   ((leaves_buffer *
                                     (node_width - leaves_buffer)) /
                                    (leaves_buffer + 1)));
        
        size_t minimum_degree = nodes_buffer;
        
        // Total number of internal nodes
        size_t nodes_count = 0;
        size_t level_count = leaves_count;
        do
        {
            level_count = ceil(float(level_count) / minimum_degree);
            nodes_count += level_count;
        } while(level_count > 1);
        
        // Width of pointers
        pointer_width = ceil(log2(std::max(nodes_count, leaves_count + 1)));
        
        assert(pointer_width <= counter_width);
        assert(pointer_width * (degree + 1) <= node_width);
        
        // Allocate space for nodes and leaves
        sizes.reset(counter_width, nodes_count * degree);
        ranks.reset(counter_width, nodes_count * degree);
        pointers.reset(pointer_width, nodes_count * (degree + 1));
        leaves.resize(leaves_count + 1);
        
        // Space for the root node
        alloc_node();
        
        // Setup the first leaf of the empty root
        //root().pointers(0) = alloc_leaf();
    }
    
    /*
     * Allocation of nodes and leaves. For now, this just means incrementing
     * a counter. 
     * TODO: switch from std::vector to std::deque to support dynamic growth
     */
    size_t bt_impl::alloc_node() {
        assert(free_node < (sizes.size() / degree));
        if(free_node == (sizes.size() / degree) - 1)
            std::cout << "Warning: nodes memory exhausted\n";
        return free_node++;
    }
    
    size_t bt_impl::alloc_leaf() {
        assert(free_leaf < leaves.size());
        
        if(free_leaf == leaves.size() - 1)
            std::cout << "Warning: leaves memory exhausted\n";
        
        return free_leaf++;
    }
    
    /*
     * The subtree_ref type is a critical abstraction in this implementation
     * of the algorithm. It allows to threat the nodes, whose data is physically
     * scattered over three different buffers, as a single unit. It also
     * takes care of the metadata about the subtree rooted at a node, metadata 
     * that are not stored in the node itself but are recursively propagated
     * during the visits. Even if inside a private interface, special care is
     * needed to ensure const-correctness, thus ensuring that the accessors
     * like access() are truly const.
     */
    template<bool Const>
    class bt_impl::subtree_ref_base
    {
    protected:
        using bt_impl_t = typename
                    std::conditional<Const, bt_impl const, bt_impl>::type;
        
        using leaf_reference = typename
        std::conditional<Const, bt_impl::leaf_t, bt_impl::leaf_t&>::type;
        
        using range_reference = typename
        std::conditional<Const, typename packed_data::const_range_reference,
                                typename packed_data::range_reference>::type;
        
        using item_reference = typename
        std::conditional<Const, typename packed_data::const_item_reference,
                                typename packed_data::item_reference>::type;
        
        // Reference to the parent bitvector structure
        bt_impl_t &_vector;
        
        // Index of the root node of the subtree
        size_t _index = 0;
        
        // Height of the subtree (distance of the root from leaves)
        size_t _height = 0;
        
        // Total size (number of bits) of the subtree
        size_t _size = 0;
        
        // Total rank (number of set bit) of the subtree
        size_t _rank = 0;
        
    public:
        subtree_ref_base(bt_impl_t &vector, size_t index, size_t height,
                         size_t size, size_t rank)
            : _vector(vector), _index(index), _height(height),
              _size(size), _rank(rank) { }
        
        subtree_ref_base(subtree_ref_base const&) = default;
        subtree_ref_base &operator=(subtree_ref_base const&) = default;
        
        // Height of the subtree
        size_t height() const { return _height; }
        
        // Index of the node/leaf in the bitvector internal data array
        size_t index() const { return _index; }
        
        // Size of the subtree
        size_t  size() const { return _size; }
        size_t &size()       { return _size; }
        
        // Rank of the subtree
        size_t  rank() const { return _rank; }
        size_t &rank()       { return _rank; }
        
        // Convenience shorthand for the degree of the subvector
        size_t degree() const { return _vector.degree; }
        
        // Pair of methods to know if the subtree is a leaf or not
        bool is_leaf() const { return _height == 0; }
        bool is_node() const { return _height > 0; }
        
        // The root node is at index zero and top height
        bool is_root() const {
            assert(_index != 0 || _height == _vector._height);
            assert(_height != _vector._height || _index == 0);
            
            return _index == 0;
        }
        
        // A full node has d + 1 children
        bool is_full() const {
            return is_leaf() ? size() == bitsize<leaf_t>()
                             : nchildren() == degree() + 1;
        }
        
        // This method creates a subtree_ref for the child at index k,
        // computing its size and its height. It's valid only if the height
        // is at least 1 (so our children are internal nodes, not leaves).
        // For accessing the leaves of level 1 nodes, use the leaf() method
        subtree_ref_base child(size_t k) const
        {
            assert(is_node());
            assert(k <= degree());
            assert(pointers(k) != 0);
            
            size_t p = pointers(k);
            size_t h = height() - 1;
            
            // Size of the subtree
            size_t s = k == 0        ? sizes(k) :
            k == degree() ? size()   - sizes(k - 1) :
            sizes(k) - sizes(k - 1);
            
            // Rank of the subtree
            size_t r = k == 0        ? ranks(k) :
            k == degree() ? rank()   - ranks(k - 1) :
            ranks(k) - ranks(k - 1);
            
            return { _vector, p, h, s, r };
        }
        
        // Access to the value of the leaf, if this subtree_ref refers to a leaf
        leaf_reference leaf() const
        {
            assert(is_leaf());
            return _vector.leaves[_index];
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
            
            size_t child = sizes().find(index);
            
            size_t new_index = index;
            if(child > 0)
                new_index -= sizes(child - 1);
            
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
        
        // Number of used keys inside the node
        size_t nchildren() const {
            if(size() == 0)
                return 0;
            
            size_t c = std::get<0>(find_insert_point(size())) + 1;
            return c;
        }
        
        // Word composed by the size fields in the interval [begin, end)
        range_reference sizes(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin >= end || begin < degree());
            assert(begin >= end || end <= degree());
            return _vector.sizes(_index * degree() + begin, _index * degree() + end);
        }
        
        // Word of the size fields.
        range_reference sizes() const { return sizes(0, degree()); }
        
        // Value of the size field at index k (with the flag bit stripped)
        //
        // NOTE: This is NOT the size of the subtree rooted at index k.
        //       To get that, use n.child(k).size()
        //
        item_reference sizes(size_t k) const { return _vector.sizes[k]; }
        
        // Word composed by the rank fields in the interval [begin, end)
        range_reference ranks(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin >= end || begin < degree());
            assert(begin >= end || end <= degree());
            return _vector.ranks(_index * degree() + begin, _index * degree() + end);
        }
        
        // Word of the rank fields
        range_reference ranks() const { return ranks(0, degree()); }
        
        // Value of the rank field at index k
        item_reference ranks(size_t k) const { return _vector.ranks[k]; }
        
        // Word composed by the pointer fields in the interval [begin, end)
        range_reference pointers(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin >= end || begin < degree() + 1);
            assert(begin >= end || end <= degree() + 1);
            return _vector.pointers(_index * (degree() + 1) + begin,
                                    _index * (degree() + 1) + end);
        }
        
        // Word of the pointer fields
        range_reference pointers() const { return pointers(0, degree() + 1); }
        
        // Value of the pointer field at index k
        item_reference pointers(size_t k) const { return _vector.pointers[k]; }
        
        // Input / Output of nodes for debugging
        friend std::ostream &operator<<(std::ostream &o, subtree_ref_base t)
        {
            if(t.is_leaf()) {
                o << "Leaf at index: " << t.index() << "\n"
                << "Size: " << t.size() << "\n"
                << "Rank: " << t.rank() << "\n"
                << "Contents: |" << to_binary(t.leaf(), 8, '|') << "|";
            } else {
                const int counter_width = int(t._vector.counter_width);
                const int pointer_width = int(t._vector.pointer_width);
                const int node_width    = int(t._vector.node_width);
                
                o << "Node at index:      " << t.index() << "\n"
                << "Total size:         " << t.size() << "\n"
                << "Total rank:         " << t.rank() << "\n"
                << "Number of children: " << t.nchildren() << "\n";
                
                o << "Sizes: |";
                o << std::setw(node_width % counter_width) << "" << "|";
                for(size_t i = t.degree() - 1; i > 0; --i)
                    o << std::setw(counter_width) << t.sizes(i) << "|";
                o << std::setw(counter_width) << t.sizes(0) << "|\n";
                o << "       |" << to_binary(t.sizes(), counter_width, '|') << "|\n";
                
                o << "Ranks: |";
                o << std::setw(node_width % counter_width) << "" << "|";
                for(size_t i = t.degree() - 1; i > 0; --i)
                    o << std::setw(counter_width) << t.ranks(i) << "|";
                o << std::setw(counter_width) << t.ranks(0) << "|\n";
                o << "       |" << to_binary(t.ranks(), counter_width, '|') << "|\n";
                
                o << "\nPtrs:  |";
                o << std::setw(int(node_width - pointer_width * (t.degree() + 1) + 1)) << "" << "|";
                for(size_t i = t.degree(); i > 0; --i)
                    o << std::setw(pointer_width) << t.pointers(i) << "|";
                o << std::setw(pointer_width) << t.pointers(0) << "|\n";
                o << "       |" << to_binary(t.pointers(), pointer_width, '|') << "|\n";
                
                if(t.height() == 1) {
                    o << "Leaves: " << t.nchildren() << "\n";
                    for(size_t i = 0; i < t.nchildren(); ++i)
                    {
                        if(!t.pointers(i))
                            o << "[x]: null\n";
                        else
                            o << "[" << t.child(i).index() << "], |" << t.child(i).size() << "|: "
                            << to_binary(t.child(i).leaf(), 8, '|') << "\n";
                    }
                }
            }
            
            return o;
        }
    };
    
    /*
     * This is the non-const version of subtree_ref, which contains the
     * member functions that can modify the nodes' data
     */
    class bt_impl::subtree_ref : public bt_impl::subtree_ref_base<false>
    {
        using Base = subtree_ref_base<false>;
        
        using Base::bt_impl_t;
        
    public:
        subtree_ref(bt_impl_t &vector, size_t index, size_t height,
                    size_t size, size_t rank)
            : Base(vector, index, height, size, rank) { }
        
        operator subtree_const_ref() const {
            return { _vector, _index, _height, _size, _rank };
        }
        
        subtree_ref(subtree_ref const&) = default;
        subtree_ref &operator=(subtree_ref const&) = default;
        
        // This method insert a new empty child into the node in position k,
        // shifting left the subsequent keys
        void insert_child(size_t k) const
        {
            assert(is_node());
            assert(k > 0);
            assert(k <= degree());
            
            if(k < degree()) {
                // FIXME:
                // There should be this assert, but since we use this method
                // inside redistribute_bits, it's not always true
                // (the node is in inconsistent state at that point)
                // assert(!is_full());
                
                size_t s = sizes(k - 1);
                size_t r = ranks(k - 1);
                
//                sizes(k - 1, degree()) <<= _vector.counter_width;
//                ranks(k - 1, degree()) <<= _vector.counter_width;
//                pointers(k, degree() + 1) <<= _vector.pointer_width;
                // TODO: To fix this step we need two things:
                // - being able to assign overlapping ranges. This has to be
                //   fixed at the bitview level
                // - have the assignment truncate the lhs if the range is bigger,
                //   rather than overflowing on the right as we do now.
                // Essentially: we have to rewrite bitview::get(). yeah -.-
                sizes(k, degree()) = sizes(k - 1, degree());
                ranks(k, degree()) = ranks(k - 1, degree());
                pointers(k + 1, degree() + 1) = pointers(k, degree() + 1);
                assert(false && "Shift of fields is still unimplemented");
                
                sizes(k - 1) = s;
                ranks(k - 1) = r;
            }
            
            pointers(k) = _height == 1 ? _vector.alloc_leaf()
                                       : _vector.alloc_node();
        }
        
        void clear_keys(size_t begin, size_t end) const
        {
            sizes(begin, end) = 0;
            ranks(begin, end) = 0;
        }
                
        // This is a modifying method because the copy needs to
        // allocate a new node
        subtree_ref copy() const
        {
            subtree_ref r = *this;
            if(is_node()) {
                r._index = _vector.alloc_node();
                
                r.sizes()    = sizes();
                r.ranks()    = ranks();
                r.pointers() = pointers();
            } else {
                r._index = _vector.alloc_leaf();
                r.leaf() = leaf();
            }
            
            return r;
        }
    };
    
    /*
     * Implementation of bitvector's interface,
     * that delegates everything to the private implementation
     */
    bitvector::bitvector(size_t capacity, size_t node_width)
        : _impl(new bt_impl(capacity, node_width)) { }
    
    bitvector::bitvector(bitvector const&other)
        : _impl(new bt_impl(*other._impl)) { }
    
    bitvector &bitvector::operator=(bitvector const&other) {
        *_impl = *other._impl;
        return *this;
    }
    
    bool bitvector::empty() const { return _impl->size == 0; }
    bool bitvector::full() const { return _impl->size == _impl->capacity; }
    
    // Debugging output pane
    std::ostream &operator<<(std::ostream &s, bitvector const&v) {
        s << "Word width         = " << v._impl->node_width << " bits\n"
          << "Capacity           = " << v._impl->capacity << " bits\n"
          << "Size counter width = " << v._impl->counter_width << " bits\n"
          << "Pointers width     = " << v._impl->pointer_width << " bits\n"
          << "Degree             = " << v._impl->degree << "\n"
          << "b                  = " << v._impl->leaves_buffer << "\n"
          << "b'                 = " << v._impl->nodes_buffer << "\n"
          << "Number of nodes    = " << v._impl->sizes.size() /
                                        v._impl->degree << "\n"
          << "Number of leaves   = " << v._impl->leaves.size() - 1 << "\n";
        return s;
    }
}
