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
# error "This is a private file, include bitvector.h instead"
#endif

#include "bits.h"
#include "packed_view.h"

#include <vector>
#include <array>
#include <cmath>
#include <type_traits>
#include <chrono>
#include <random>
#include <iomanip>

namespace bv
{
    using std::floor;
    using std::ceil;
    using std::log2;
    using std::sqrt;
    using std::pow;
    using std::min;
    using std::max;
    using std::log10;
    
    /*
     * Private implementation class for bitvector
     */
    template<size_t W>
    struct bt_impl
    {
        /*
         * Types
         */
        template<bool Const>
        class subtree_ref_base;
        
        class subtree_ref;
        using subtree_const_ref = subtree_ref_base<true>;
        
        // Here we define the container used for the storage of nodes and leaves
        // TODO: Change it do std::deque
        template<typename T>
        using data_container = std::vector<T>;
        
        using packed_data = packed_view<data_container>;
        using field_type = typename packed_data::value_type;
        
        static constexpr size_t leaf_bits = W;
        using leaf_t = bitarray<leaf_bits>;
        using leaf_reference = typename data_container<leaf_t>::reference;
        using const_leaf_reference = typename data_container<leaf_t>::const_reference;
        
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
        data_container<leaf_t> leaves;
        
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
        
        // Creation of root node ref
        subtree_ref       root();
        subtree_const_ref root() const;
        
        // Read-only access to the bits data
        bool access(subtree_const_ref t, size_t index) const;
        
        // Set a bit
        void set(subtree_ref t, size_t index, bool bit);
        
        // Insertion of a bit
        void insert(subtree_ref t, size_t index, bool bit);
        
        // Find children for the redistribution
        std::tuple<size_t, size_t, size_t>
        find_adjacent_children(subtree_const_ref t, size_t child);
        
        // Reset of children counters of a node, needed by insert() & co.
        void clear_children_counters(subtree_ref t,
                                     size_t begin, size_t end) const;
        
        // Redistribution of bits in the leaves
        void redistribute_bits(subtree_ref t,
                               size_t begin, size_t end, size_t count);
        
        // Redistribution of keys in the nodes
        void redistribute_keys(subtree_ref t,
                               size_t begin, size_t end, size_t count);
    };
    
    
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
    template<size_t W>
    template<bool Const>
    class bt_impl<W>::subtree_ref_base
    {
    protected:
        using bt_impl_t = add_const_if_t<Const, bt_impl>;
        
        using leaf_reference = conditional_t<Const,
                                             bt_impl::const_leaf_reference,
                                             bt_impl::leaf_reference>;
        
        using range_reference =
               conditional_t<Const, typename packed_data::const_range_reference,
                                    typename packed_data::range_reference>;
        
        using item_reference =
                conditional_t<Const, typename packed_data::const_item_reference,
                                     typename packed_data::item_reference>;
        
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
        
        bt_impl_t &vector() const { return _vector; }
        
        // Index of the node/leaf in the bitvector internal data array
        size_t index() const { return _index; }
        
        // Height of the subtree
        size_t height() const { return _height; }
        
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
            assert(_index != 0 || _height == _vector.height);
            assert(_height != _vector.height || _index == 0);
            
            return _index == 0;
        }
        
        // A full node has d + 1 children
        bool is_full() const {
            return is_leaf() ? size() == leaf_bits
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
            return _vector.sizes(_index * degree() + begin,
                                 _index * degree() + end);
        }
        
        // Word of the size fields.
        range_reference sizes() const { return sizes(0, degree()); }
        
        // Value of the size field at index k (with the flag bit stripped)
        //
        // NOTE: This is NOT the size of the subtree rooted at index k.
        //       To get that, use n.child(k).size()
        //
        item_reference sizes(size_t k) const {
            return _vector.sizes[_index * degree() + k];
        }
        
        // Word composed by the rank fields in the interval [begin, end)
        range_reference ranks(size_t begin, size_t end) const
        {
            assert(is_node());
            assert(begin >= end || begin < degree());
            assert(begin >= end || end <= degree());
            return _vector.ranks(_index * degree() + begin,
                                 _index * degree() + end);
        }
        
        // Word of the rank fields
        range_reference ranks() const { return ranks(0, degree()); }
        
        // Value of the rank field at index k
        item_reference ranks(size_t k) const {
            return _vector.ranks[_index * degree() + k];
        }
        
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
        item_reference pointers(size_t k) const {
            return _vector.pointers[_index * (degree() + 1) + k];
        }
        
        // Input / Output of nodes for debugging
        friend std::ostream &operator<<(std::ostream &o, subtree_ref_base t)
        {
            if(t.is_leaf()) {
                o << "Leaf at index: " << t.index() << "\n"
                  << "Size: " << t.size() << "\n"
                  << "Rank: " << t.rank() << "\n"
                  << "Contents: |" << to_binary(t.leaf(), 8, '|') << "|";
            } else {
                int field_width = std::max(log10(t.size()),
                                           log10(t._vector.used_leaves())) + 4;
                
                o << "Node at index:      " << t.index() << "\n"
                  << "Total size:         " << t.size() << "\n"
                  << "Total rank:         " << t.rank() << "\n"
                  << "Number of children: " << t.nchildren() << "\n";
                
                o << "Sizes: |" << std::setw(field_width + 1) << "|";
                for(size_t i = t.degree() - 1; i > 0; --i)
                    o << std::setw(field_width) << t.sizes(i) << "|";
                o << std::setw(field_width) << t.sizes(0) << "|\n";
                
                o << "Ranks: |" << std::setw(field_width + 1) << "|";
                for(size_t i = t.degree() - 1; i > 0; --i)
                    o << std::setw(field_width) << t.ranks(i) << "|";
                o << std::setw(field_width) << t.ranks(0) << "|\n";
                
                o << "\nPtrs:  |";
                for(size_t i = t.degree(); i > 0; --i)
                    o << std::setw(field_width) << t.pointers(i) << "|";
                o << std::setw(field_width) << t.pointers(0) << "|\n";
                
                if(t.height() == 1) {
                    o << "Leaves: " << t.nchildren() << "\n";
                    for(size_t i = 0; i < t.nchildren(); ++i)
                    {
                        if(!t.pointers(i))
                            o << "[x]: null\n";
                        else
                            o << "#" << i
                              << ", [" << t.child(i).index()
                              << "], s = " << t.child(i).size()
                              << ", r = " << ssize_t(t.child(i).rank()) << ": "
                              << to_binary(t.child(i).leaf(), 8, '|') << "\n";
                    }
                }
            }
            
            return o;
        }
    };
    
    /*
     * Implementatin of bt_impl members
     */
    
    /*
     * This is the non-const version of subtree_ref, which contains the
     * member functions that can modify the nodes' data
     */
    template<size_t W>
    class bt_impl<W>::subtree_ref
        : public bt_impl<W>::subtree_ref_base<false>
    {
        using Base = subtree_ref_base<false>;
        
        using typename Base::bt_impl_t;
        using Base::_vector;
        using Base::_index;
        using Base::_height;
        using Base::_size;
        using Base::_rank;
        
    public:
        using Base::sizes;
        using Base::ranks;
        using Base::pointers;
        using Base::degree;
        using Base::is_node;
        using Base::leaf;
        
    public:
        subtree_ref(bt_impl_t &vector, size_t index, size_t height,
                    size_t size, size_t rank)
            : Base(vector, index, height, size, rank) { }
        
        subtree_ref(subtree_ref_base<false> const&r)
            : Base(r.vector(), r.index(), r.height(), r.size(), r.rank()) { }
        
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
                
                sizes(k, degree()) = sizes(k - 1, degree());
                ranks(k, degree()) = ranks(k - 1, degree());
                pointers(k + 1, degree() + 1) = pointers(k, degree() + 1);
                
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
                
                r.sizes()  = sizes();
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
     * Parameters are computed according to the required maximum capacity.
     * Please refer to the paper for a detailed explaination.
     */
    template<size_t W>
    bt_impl<W>::bt_impl(size_t N, size_t Wn)
    {
        capacity = N;
        node_width = Wn;
        
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
                                     (leaf_bits - leaves_buffer)) /
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
        root().pointers(0) = alloc_leaf();
    }
    
    /*
     * Allocation of nodes and leaves. For now, this just means incrementing
     * a counter.
     * TODO: switch from std::vector to std::deque to support dynamic growth
     */
    template<size_t W>
    size_t bt_impl<W>::alloc_node() {
        assert(free_node < (sizes.size() / degree));
        return free_node++;
    }
    
    template<size_t W>
    size_t bt_impl<W>::alloc_leaf() {
        assert(free_leaf < leaves.size());        
        return free_leaf++;
    }
    
    /*
     * Here we create the subtree_ref that refer to the root of the tree,
     * at the top of the recursion. All other subtree_ref are derived from
     * this one using the child() member function.
     * Note that height, size and rank are copied, so the ref can't stay
     * in sync with the reality. In a lot of places inside the execution of
     * insert(), for example, the size() and rank() of the subtree_ref become
     * temporarily wrong.
     */
    template<size_t W>
    auto bt_impl<W>::root() -> subtree_ref {
        return { *this, 0, height, size, rank };
    }
    
    template<size_t W>
    auto bt_impl<W>::root() const -> subtree_const_ref {
        return { *this, 0, height, size, rank };
    }
    
    /*
     * This is the implementation of the bit search into the tree.
     * No big deal here, it's only a tree search.
     */
    template<size_t W>
    bool bt_impl<W>::access(subtree_const_ref t, size_t index) const
    {
        assert(index < t.size() && "Index out of bounds");
        
        if(t.is_leaf()) // We have a leaf
            return t.leaf().get(index);
        else { // We're in a node
            size_t child, new_index;
            std::tie(child, new_index) = t.find(index);
            
            return access(t.child(child), new_index);
        }
    }
    
    /*
     * Setting a bit. 
     * The structure is identical to access, nothing to see here folks.
     */
    template<size_t W>
    void bt_impl<W>::set(subtree_ref t, size_t index, bool bit)
    {
        assert(index < t.size() && "Index out of bounds");
        
        if(t.is_leaf()) // We have a leaf
            t.leaf().set(index, bit);
        else {
            size_t child, new_index;
            std::tie(child, new_index) = t.find(index);
            
            return set(t.child(child), new_index, bit);
        }
    }
    
    /*
     * This is the entry point of the insertion algorithm
     * For details on the algorithm itself, see the paper.
     * From an implementation point of view, the central point is the
     * subtree_ref class, which tries to hide the fact the the nodes' data
     * is packed into variable sized bitfields inside three different buffers.
     * The final result should be that, if you only look at this insert() 
     * method, the algorithm could roughly seem like a semi-standard, non-packed
     * B+-tree, with all the weird bit operations hidden in subtree_ref or in 
     * lower layers.
     */
    template<size_t W>
    void bt_impl<W>::insert(subtree_ref t, size_t index, bool bit)
    {
        assert(index <= t.size() && "Index out of bounds");
        
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
            t.sizes() = t.size();
            t.ranks() = t.rank();
            t.pointers() = 0;
            t.pointers(0) = old_root.index();
            
            // The only point in the algorithm were the height increases
            ++height;
            
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
                if(count >= leaves_buffer * (leaf_bits - leaves_buffer))
                    // We need to split. The node should not be full
                    t.insert_child(end++);
                
                // redistribute
                redistribute_bits(t, begin, end, count);
                
                // It's important to stay in shape and not get too fat
                if(size >= (leaves_buffer * (leaf_bits - leaves_buffer))) {
                    for(size_t k = begin; k < end; ++k) {
                        size_t minsize = (leaves_buffer * (leaf_bits - leaves_buffer)) /
                                         (leaves_buffer + 1);
                        size_t size = t.child(k).size();
                        assert(size >= minsize);
                        // Silence unused warnings in release mode
                        (void)minsize;
                        (void)size;
                    }
                }
                
                // Search again where to insert the bit
                std::tie(child, new_index) = t.find_insert_point(index);
            }
            
            // 2. Update counters
            size += 1;
            rank += bit;
            t.sizes(child, degree) += 1;
            t.ranks(child, degree) += bit;
            
            // 3. Insert the bit
            t.child(child).leaf().insert(new_index, bit);
        }
        else // We'll have another node
        {
            // 1. Check if we need a split and/or a redistribution of keys
            if(t.child(child).is_full())
            {
                // The node is full, we need a redistribution
                size_t begin, end, count;
                std::tie(begin, end, count) = find_adjacent_children(t, child);
                
                if(count / (nodes_buffer + 1) >= nodes_buffer)
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
            
            t.sizes(child, degree) += 1;
            t.ranks(child, degree) += bit;
            
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
    template<size_t W>
    std::tuple<size_t, size_t, size_t>
    bt_impl<W>::find_adjacent_children(subtree_const_ref t, size_t child)
    {
        const bool is_leaf = t.child(child).is_leaf();
        const size_t buffer = is_leaf ? leaves_buffer : nodes_buffer;
        const size_t max_count = is_leaf ? leaf_bits : (degree + 1);
        const auto count = [&](size_t i) {
            return t.pointers(i) == 0 ? max_count :
                   is_leaf            ? leaf_bits - t.child(i).size() :
                                        (degree + 1) - t.child(i).nchildren();
        };
        
        size_t begin = child >= buffer ? child - buffer + 1 : 0;
        size_t end = std::min(begin + buffer, degree + 1);
        
        size_t freeslots = 0;
        size_t maxfreeslots = 0;
        std::pair<size_t, size_t> window = { begin, end };
        
        // Sum for the initial window
        for(size_t i = begin; i < end; ++i)
            freeslots += count(i);
        maxfreeslots = freeslots;
        
        // Slide the window
        while(begin < child && end < t.nchildren())
        {
            freeslots = freeslots - count(begin) + count(end);
            
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
    // Note that at the end of this function, the subtree_ref is in an
    // inconsistent state regarding its size, so we have to be careful to
    // not call functions such as nchildren() and is_full().
    // This function is used only in redistribute_bits() and redistribute_keys(),
    // to prepare the redistribution. In these functions we already know how
    // many children we want to iterate on, so we don't need nchildren()
    //
    template<size_t W>
    void bt_impl<W>::clear_children_counters(subtree_ref t,
                                          size_t begin, size_t end) const
    {
        size_t keys_end = std::min(end, degree);
        size_t last_size = end < degree ? t.sizes(end - 1) : size;
        size_t last_rank = end < degree ? t.ranks(end - 1) : rank;
        size_t prev_size = begin > 0 ? t.sizes(begin - 1) : 0;
        size_t prev_rank = begin > 0 ? t.ranks(begin - 1) : 0;
        
        assert(last_size >= prev_size);
        assert(last_rank >= prev_rank);
        
        t.sizes(begin, keys_end)   = prev_size;
        t.ranks(begin, keys_end)   = prev_rank;
        
        t.sizes(keys_end, degree) -= last_size - prev_size;
        t.ranks(keys_end, degree) -= last_rank - prev_rank;
    }
    
    // FIXME: rewrite using a temporary space of two words instead
    //        of the whole buffer, thus avoiding the dynamic allocation
    template<size_t W>
    void bt_impl<W>::redistribute_bits(subtree_ref t,
                                    size_t begin, size_t end, size_t count)
    {
        size_t b = end - begin; // Number of children to use
        size_t bits_per_leaf = count / b; // Average number of bits per leaf
        size_t rem           = count % b; // Remainder
        
        assert(b == leaves_buffer || b == leaves_buffer + 1);
        
        // Here we use the existing abstraction of packed_view
        // to accumulate all the bits into a temporary buffer, and
        // subsequently redistribute them to the leaves
        bitview<std::vector> bits(count);
        
        for(size_t i = begin, p = 0; i < end; ++i) {
            if(t.pointers(i) != 0) {
                size_t step = t.child(i).size();
                leaf_reference leaf = t.child(i).leaf();
                bits.copy(leaf, 0, leaf.size(), p, p + step);
                p += step;
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
            //FIXME: remove insert_child() like in redistribute_keys?
            if(t.pointers(i) == 0)
                t.insert_child(i);
            
            // Take the bits out of the buffer put them back into the leaf
            leaf_reference leaf = t.child(i).leaf();
            leaf.clear();
            leaf.copy(bits, p, p + n, 0, leaf.size());
            
            // Increment the counters
            t.sizes(i, degree) += n;
            t.ranks(i, degree) += leaf.popcount();
            
            // Count of the copied bits
            p += n;
            count -= n;
        }
        
        assert(count == 0);
    }
    
    // FIXME: rewrite using a temporary space of two words instead
    //        of the whole buffer, thus avoiding the dynamic allocation
    template<size_t W>
    void bt_impl<W>::redistribute_keys(subtree_ref t,
                                    size_t begin, size_t end, size_t count)
    {
        size_t b = end - begin;
        size_t keys_per_node = count / b;
        size_t rem           = count % b;
        
        assert(b == nodes_buffer || b == nodes_buffer + 1);
        
        struct pointer {
            size_t size;
            size_t rank;
            size_t ptr;
        };
        
        std::vector<pointer> pointers;
        pointers.reserve(nodes_buffer * (degree + 1));
        
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
                t.pointers(i) = alloc_node();
            
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
                t.child(i).sizes(j, degree) += s;
                t.child(i).ranks(j, degree) += r;
                
                childsize += s;
                childrank += r;
            }
            
            t.sizes(i, degree) += childsize;
            t.ranks(i, degree) += childrank;
            
            count -= n;
            p += n;
        }
        
        assert(count == 0);
    }
    
    /*
     * Implementation of bitvector's interface,
     * that delegates everything to the private implementation
     */
    template<size_t W>
    inline
    bitvector_t<W>::bitvector_t(size_t capacity, size_t node_width)
        : _impl(new bt_impl<W>(capacity, node_width)) { }
    
    template<size_t W>
    inline
    bitvector_t<W>::bitvector_t(bitvector_t const&other)
        : _impl(new bt_impl<W>(*other._impl)) { }
    
    template<size_t W>
    inline
    bitvector_t<W> &bitvector_t<W>::operator=(bitvector_t const&other) {
        *_impl = *other._impl;
        return *this;
    }
    
    template<size_t W>
    inline
    bool bitvector_t<W>::empty() const { return _impl->size == 0; }
    
    template<size_t W>
    inline
    bool bitvector_t<W>::full() const { return _impl->size == _impl->capacity; }
    
    template<size_t W>
    inline
    bool bitvector_t<W>::access(size_t index) const {
        return _impl->access(_impl->root(), index);
    }
    
    template<size_t W>
    inline
    void bitvector_t<W>::set(size_t index, bool bit) {
        return _impl->set(_impl->root(), index, bit);
    }
    
    template<size_t W>
    inline
    void bitvector_t<W>::insert(size_t index, bool bit) {
        _impl->insert(_impl->root(), index, bit);
    }
    
    template<size_t W>
    inline
    auto bitvector_t<W>::operator[](size_t index) -> reference {
        assert(index < size());
        return { *this, index };
    }
    
    template<size_t W>
    inline
    auto bitvector_t<W>::operator[](size_t index) const -> const_reference {
        assert(index < size());
        return { *this, index };
    }
    
    template<size_t W>
    inline
    void bitvector_t<W>::push_back(bool bit) {
        insert(size(), bit);
    }
    
    template<size_t W>
    inline
    void bitvector_t<W>::push_front(bool bit) {
        insert(0, bit);
    }
    
    /*
     * Reference types for the access operators
     */
    template<size_t W>
    class bitvector_t<W>::const_reference
    {
        friend class bitvector_t;
        
        bitvector_t const&_v;
        size_t _index;
        
        const_reference(bitvector_t const&v, size_t index)
        : _v(v), _index(index) { }
    public:
        const_reference(const_reference const&) = default;
        
        operator bool() const {
            return _v.access(_index);
        }
    };
    
    template<size_t W>
    class bitvector_t<W>::reference
    {
        friend class bitvector_t;
        
        bitvector_t &_v;
        size_t _index;
        
        reference(bitvector_t &v, size_t index)
        : _v(v), _index(index) { }
    public:
        reference(reference const&) = default;
        
        reference &operator=(const_reference const&ref) {
            _v.set(_index, ref._v.access(ref._index));
            return *this;
        }
        
        reference &operator=(bool bit) {
            _v.set(_index, bit);
            return *this;
        }
        
        operator const_reference() const {
            return { _v, _index };
        }
        
        operator bool() const {
            return _v.access(_index);
        }
    };
    
    /*
     * Test and debugging functions
     */
    template<size_t W>
    void bitvector_t<W>::test(std::ostream &stream, size_t N, size_t Wn,
                         bool dumpinfo, bool dumpnode, bool dumpcontents)
    {
        using std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;
        using std::chrono::duration;
        
        bitvector_t v(N, Wn);
        
        if(dumpinfo)
            stream << v << "\n";
        
        size_t nbits = N;
        
        std::mt19937 engine(42);
        std::vector<std::pair<size_t, bool>> indexes;
        bool b = true;
        for(size_t i = 0; i < nbits; ++i) {
            std::uniform_int_distribution<size_t> dist(0, i);
            indexes.emplace_back(dist(engine), b = !b);
        }
        
        auto t1 = high_resolution_clock::now();
        for(size_t i = 0; i < nbits - 1; ++i)
            v.insert(indexes.at(i).first, indexes.at(i).second);
        v.insert(indexes.at(nbits - 1).first, indexes.at(nbits - 1).second);
        auto t2 = high_resolution_clock::now();
        
        if(dumpnode) {
            stream << "\n" << v._impl->root() << "\n";
            if(v._impl->root().height() > 1) {
                for(size_t i = 0; i < v._impl->root().nchildren(); ++i) {
                    if(v._impl->root().pointers(i) == 0)
                        stream << "Child " << i << " is null\n";
                    else
                        stream << v._impl->root().child(i) << "\n";
                }
            }
        }
        
        if (dumpcontents) {
            for(size_t i = 0; i < nbits; ++i) {
                if(i && i % 8 == 0)
                    stream << " ";
                if(i && i % 64 == 0)
                    stream << "\n";
                stream << v.access(i);
            }
            stream << "\n\n";
        }
      
        double total = duration_cast<duration<double, std::ratio<1>>>(t2 - t1).count();
        stream << "Inserted " << nbits << " bits (W = " << Wn << ") in "
               << total << "s\n";
    }
    
    template<size_t W>
    inline
    auto bitvector_t<W>::info() const -> info_t
    {
        return { _impl->capacity,
            _impl->size,
            _impl->node_width,
            _impl->counter_width,
            _impl->pointer_width,
            _impl->degree,
            _impl->nodes_buffer,
            _impl->sizes.size() / _impl->degree,
            _impl->leaves.size()
        };
    }
    
    template<size_t W>
    std::ostream &operator<<(std::ostream &s, bitvector_t<W> const&v) {
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
