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

#ifndef PACKED_BITVECTOR_H
# error "This is a private file, include bitvector.h instead"
#endif

#include "bits.h"
#include "packed_view.h"

#include <vector>
#include <deque>
#include <array>
#include <cmath>
#include <type_traits>
#include <chrono>
#include <random>
#include <iomanip>

namespace bv
{
    namespace internal {
        using std::floor;
        using std::ceil;
        using std::log2;
        using std::sqrt;
        using std::pow;
        using std::min;
        using std::max;
        using std::log10;
        using std::tie;
        
        /*
         * Private implementation class for bitvector
         */
        template<size_t W, allocation_policy_t AllocPolicy>
        struct bt_impl
        {
            /*
             * Types
             */
            template<bool Const>
            class subtree_ref_base;
            
            class subtree_ref;
            using subtree_const_ref = subtree_ref_base<true>;
            
            // Here we define the container used for the storage of nodes and
            // leaves
            template<typename T>
            using data_container = conditional_t<AllocPolicy == alloc_on_demand,
                                                 std::deque<T>,
                                                 std::vector<T>>;
            
            using packed_data = packed_view<data_container>;
            using field_type = typename packed_data::word_type;
            
            static constexpr size_t leaf_bits = W;
            using leaf_t = bitarray<leaf_bits>;
            using leaf_reference = typename data_container<leaf_t>::reference;
            using const_leaf_reference = typename
                                         data_container<leaf_t>::const_reference;
            
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
            size_t capacity = 0;
            
            // Number of bits used for a node
            size_t node_width = 0;
            
            // Bit width of the nodes' counters inside nodes' words
            size_t counter_width = 0;
            
            // Bit width of nodes' pointers
            size_t pointer_width = 0;
            
            // Number of counters per node, refered as d in the paper
            size_t degree = 0;
            
            // Number of nodes used for redistribution of keys/bits when
            // the insertion find a full node/leaf
            size_t buffer = 0;
            
            // Number of leaves needed in the worst-case
            size_t leaves_count = 0;
            
            // Number of internal nodes needed in the worst-case
            size_t nodes_count = 0;
            
            // Current number of bits stored in the bitvector
            size_t size = 0;
            
            // Total rank of the bitvector
            size_t rank = 0;
            
            // Height of the tree (distance of the root node from the leaves)
            size_t height = 0;
            
            // Index of the first unused node in the nodes arrays
            size_t free_node = 0;
            
            // Index of the first unused leaf in the leaves array
            // Starts from 1 because of the unused sentinel for null pointers to
            // leaves (not needed for internal nodes)
            size_t free_leaf = 0;
            
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
            
            // We don't want trivial accessors in the private interface, but
            // these two do something different.
            size_t used_leaves() const { return free_leaf; }
            size_t used_nodes() const { return free_node; }
            
            // When all the capacity fits into a single leaf we switch to a
            // cheaper mode
            bool small() const { return capacity <= leaf_bits; }
            
            // Reserve space for the given number of nodes
            void reserve_nodes(size_t nodes);
            
            // Nodes and leaves allocation
            // If the number of nodes/leaves requested is more than 1,
            // the allocated nodes are contiguous
            size_t alloc_node();
            size_t alloc_leaf();
            
            // Creation of root node ref
            subtree_ref       root();
            subtree_const_ref root() const;
            
            // Read-only access to the bits data
            bool access(subtree_const_ref t, size_t index) const;
            
            // Number of set bits before index, i.e. range [0, index)
            size_t getrank(subtree_const_ref t, size_t index, size_t acc) const;
            
            // Set a bit
            bool set(subtree_ref t, size_t index, bool bit);
            
            // Insertion of a bit
            void insert(subtree_ref t, size_t index, bool bit);
            
            // Find children for the redistribution
            std::tuple<size_t, size_t, size_t>
            find_adjacent_children(subtree_const_ref t, size_t child);
            
            // Reset of children counters of a node, needed by insert() & co.
            void clear_children_counters(subtree_ref t,
                                         size_t begin, size_t end) const;
            
            // Limit to decide if a redistribution needs a split or not
            size_t split_limit(subtree_ref t);
            
            // Entry point for the redistribution procedures
            void redistribute(subtree_ref t,
                              size_t begin, size_t end, size_t count);
            
            // Redistribution of bits in the leaves
            void redistribute_bits(subtree_ref t,
                                   size_t begin, size_t end, size_t count);
            
            // Redistribution of keys in the nodes
            void redistribute_keys(subtree_ref t,
                                   size_t begin, size_t end, size_t count);
        };
        
        
        /*
         * The subtree_ref type is a critical abstraction in this implementation
         * of the algorithm. It allows to threat the nodes, whose data is
         * physically scattered over three different buffers, as a single unit.
         * It also takes care of the metadata about the subtree rooted at a
         * node, metadata that are not stored in the node itself but are
         * recursively propagated during the visits. Even if inside a private
         * interface, special care is needed to ensure const-correctness,
         * thus ensuring that the accessors like access() are truly const.
         */
        template<size_t W, allocation_policy_t AP>
        template<bool Const>
        class bt_impl<W, AP>::subtree_ref_base
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
                conditional_t<Const, typename packed_data::const_reference,
                                     typename packed_data::reference>;
            
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
            
            // Access to the value of the leaf, if this subtree_ref refers to a
            // leaf
            leaf_reference leaf() const
            {
                assert(is_leaf());
                return _vector.leaves[_index];
            }
            
            // Finds the subtree where the bit at the given index can be
            // inserted.
            // The position found by this function is suitable for insertion,
            // if you need lookup, use find().
            //
            // The returned pair contains:
            //  - The index of the subtree
            //  - The new index, relative to the subtree, where to insert the bit
            //
            // For the bit tricks used in this step see the paper
            std::pair<size_t, size_t>
            find_insert_point(size_t index) const
            {
                assert(is_node());
                
                using word_type = typename packed_data::word_type;
                
                size_t len = degree();
                const word_type field_mask   = _vector.sizes.field_mask();
                const word_type flag_mask    = _vector.sizes.flag_mask();
                const size_t width           = _vector.counter_width;
                const size_t fields_per_word = _vector.sizes.elements_per_word();
                const size_t rem             = len % fields_per_word;
                
                ensure_bitsize(index, width - 1);
                
                size_t child = len;
                for(size_t step, p = 0; p < degree(); len -= step, p += step)
                {
                    step = len < fields_per_word ? rem : fields_per_word;
                    
                    word_type word = word_type(sizes(p, p + step)) |
                                     flag_mask;
                    
                    child -= popcount(lowbits(flag_mask &
                                              (word - index * field_mask),
                                              step * width));
                }
                
                assert(len == 0);
                
                size_t new_index = index;
                if(child > 0)
                    new_index -= sizes(child - 1);
                
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
                tie(child, new_index) = find_insert_point(index);
                
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
                check_valid_range(begin, end, degree());
                
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
                check_valid_range(begin, end, degree());
                
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
                check_valid_range(begin, end, degree() + 1);
                
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
                    int field_width = int(max(log10(t.size()),
                                              log10(t._vector.used_leaves())))
                                      + 4;
                    
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
        template<size_t W, allocation_policy_t AP>
        class bt_impl<W, AP>::subtree_ref
            : public bt_impl<W, AP>::template subtree_ref_base<false>
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
            subtree_ref(bt_impl_t &vector_, size_t index_, size_t height_,
                        size_t size_, size_t rank_)
                : Base(vector_, index_, height_, size_, rank_) { }
            
            subtree_ref(subtree_ref_base<false> const&r)
                : Base(r.vector(), r.index(), r.height(),
                       r.size(), r.rank()) { }
            
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
        template<size_t W, allocation_policy_t AP>
        bt_impl<W, AP>::bt_impl(size_t N, size_t Wn)
        {
            capacity = N;
            
            // When all the maximum capacity fits in a leaf, we act consequently
            if(small()) {
                // We only need one leaf
                leaves_count = 1;
                alloc_leaf();
                return;
            }
            
            node_width = Wn;
            
            // The + 1 at the end is for the extra flag bit
            counter_width = size_t(floor(log2(capacity)) + 1) + 1;
            
            degree = node_width / counter_width;
            
            for(buffer = max(size_t(ceil(sqrt(degree))), size_t(1));
                floor((degree + 1)/buffer) < buffer;
                --buffer);
            
            // Maximum number of leaves needed in the worst case
            leaves_count = size_t(ceil(capacity / ((buffer *
                                                    (leaf_bits - buffer))
                                                  / (buffer + 1))))
                           + 1; // for the sentinel leaf at index 0
            
            size_t minimum_degree = buffer;
            
            // Total number of internal nodes
            nodes_count = 0;
            size_t level_count = leaves_count;
            do
            {
                level_count = ceildiv(level_count, minimum_degree);
                nodes_count += level_count;
            } while(level_count > 1);
            
            // For values of small capacity relatively to the leaves and nodes
            // bit size, the buffer could be greater than the maximum count.
            leaves_count = max(leaves_count, buffer + 1);
            nodes_count = max(nodes_count, buffer + 1);
            
            // Width of pointers
            pointer_width = size_t(ceil(log2(max(nodes_count, leaves_count))));
            
            assert(pointer_width <= counter_width);
            assert(pointer_width * (degree + 1) <= node_width);
            
            // Allocate all the needed memory ahead of time if the policy says so
            if(AP == alloc_immediatly) {
                reserve_nodes(nodes_count);
                leaves.resize(leaves_count);
            }
            
            // Allocate space for the root node and its first leaf
            height = 1; // Initial height is 1
            alloc_node(); // Initial root node
            alloc_leaf(); // Sentinel leaf for position zero
            root().pointers(0) = alloc_leaf(); // First leaf
        }
        
        /*
         * Allocation of nodes and leaves.
         * Resizing the packed_view's at each new node could seem wasteful but
         * note that underlying containers are smarter.
         */
        template<size_t W, allocation_policy_t AP>
        void bt_impl<W, AP>::reserve_nodes(size_t nodes)
        {
            sizes.reset(counter_width, nodes * degree);
            ranks.reset(counter_width, nodes * degree);
            pointers.reset(pointer_width, nodes * (degree + 1));
        }
        
        template<size_t W, allocation_policy_t AP>
        size_t bt_impl<W, AP>::alloc_node() {
            assert(used_nodes() < nodes_count &&
                   "Maximum number of nodes exceeded");
            
            size_t node = free_node++;
            
            if(AP == alloc_on_demand)
                reserve_nodes(used_nodes());
            
            return node;
        }
        
        template<size_t W, allocation_policy_t AP>
        size_t bt_impl<W, AP>::alloc_leaf() {
            assert(used_leaves() < leaves_count &&
                   "Maximum number of leaves exceeded");
            
            size_t leaf = free_leaf++;
            
            if(AP == alloc_on_demand)
                leaves.resize(used_leaves());
            
            return leaf;
        }
        
        /*
         * Here we create the subtree_ref that refer to the root of the tree,
         * at the top of the recursion. All other subtree_ref are derived from
         * this one using the child() member function.
         * Note that height, size and rank are copied, so the ref can't stay
         * in sync with the reality. In a lot of places inside the execution of
         * insert(), for example, the size() and rank() of the subtree_ref
         * become temporarily wrong.
         */
        template<size_t W, allocation_policy_t AP>
        auto bt_impl<W, AP>::root() -> subtree_ref {
            return { *this, 0, height, size, rank };
        }
        
        template<size_t W, allocation_policy_t AP>
        auto bt_impl<W, AP>::root() const -> subtree_const_ref {
            return { *this, 0, height, size, rank };
        }
        
        /*
         * This is the implementation of the bit search into the tree.
         * No big deal here, it's only a tree search.
         */
        template<size_t W, allocation_policy_t AP>
        bool bt_impl<W, AP>::access(subtree_const_ref t, size_t index) const
        {
            assert(index < t.size() && "Index out of bounds");
            
            if(t.is_leaf()) // We have a leaf
                return t.leaf()[index];
            else { // We're in a node
                size_t child, new_index;
                tie(child, new_index) = t.find(index);
                
                return access(t.child(child), new_index);
            }
        }
        
        template<size_t W, allocation_policy_t AP>
        size_t bt_impl<W, AP>::getrank(subtree_const_ref t,
                                       size_t index, size_t prevrank) const
        {
            assert(index <= t.size() && "Index out of bounds");
            
            // Short circuit for special cases
            if(index == t.size())
                return t.rank() + prevrank;
            
            if(index == 0)
                return prevrank;
            
            if(t.is_leaf())
                return t.leaf().popcount(0, index) + prevrank;
            else {
                size_t child, new_index;
                tie(child, new_index) = t.find_insert_point(index);
                
                size_t pr = child == 0 ? prevrank
                                       : prevrank + t.ranks(child - 1);
                
                return getrank(t.child(child), new_index, pr);
            }
        }
        
        /*
         * Setting a bit.
         * The structure is identical to access, but we have to go up the tree
         * updating the rank counters
         */
        template<size_t W, allocation_policy_t AP>
        bool bt_impl<W, AP>::set(subtree_ref t, size_t index, bool bit)
        {
            assert(index < t.size() && "Index out of bounds");
            
            if(t.is_leaf()) {
                // We have a leaf
                // We need to read the previous value to know if we have to
                // update the ranks
                bool b = t.leaf()[index];
                if(b != bit) {
                    if(b && !bit)
                        rank -= 1;
                    if(!b && bit)
                        rank += 1;
                    
                    t.leaf()[index] = bit;
                }
                return b;
            } else {
                size_t child, new_index;
                tie(child, new_index) = t.find(index);
                
                bool b = set(t.child(child), new_index, bit);
                if(b && !bit)
                    t.ranks(child, degree()) -= 1;
                if(!b && bit)
                    t.ranks(child, degree()) += 1;
                return b;
            }
        }
        
        /*
         * This is the entry point of the insertion algorithm
         * For details on the algorithm itself, see the paper.
         * From an implementation point of view, the central point is the
         * subtree_ref class, which tries to hide the fact the the nodes' data
         * is packed into variable sized bitfields inside three different 
         * buffers.
         * The final result should be that, if you only look at this insert()
         * method, the algorithm could roughly seem like a semi-standard,
         * non-packed B+-tree, with all the weird bit operations hidden in 
         * subtree_ref or in lower layers.
         */
        template<size_t W, allocation_policy_t AP>
        void bt_impl<W, AP>::insert(subtree_ref t, size_t index, bool bit)
        {
            assert(index <= t.size() && "Index out of bounds");
            assert(size < capacity);
            
            // If we see a full node in this point it must be the root,
            // otherwise we've violated our invariants.
            // So, since it's a root and it's full, we prepare the split
            // by allocating a new node and swapping it with the old root,
            // thus ensuring the root is always at index zero.
            // Then we go ahead pretending we started the insertion from the
            // new root, so we don't duplicate the node splitting code
            // below
            // In small mode this should not happen, since this means that
            // the whole bitvector is full and we can't insert anything at all.
            if(t.is_full())
            {
                assert(t.is_root());
                assert(!small());
                
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
            
            // Hereafter we assume the node is not full
            
            // If we're inserting into a leaf, it's very simple.
            if(t.is_leaf())
            {
                // 1 - Update the counters
                size += 1;
                rank += bit;
                
                // 2 - Insert the bit
                return t.leaf().insert(index, bit);
            }
            
            // Else, we are in an internal node

            // 1 - Find where we have to insert this bit
            size_t child, new_index;
            tie(child, new_index) = t.find_insert_point(index);
            
            // 2 - Check if we need a split and/or a redistribution of bits
            if(t.child(child).is_full())
            {
                // 2.1 - Find the range of children that will be the target
                //       of the redistribution
                size_t begin, end, count;
                tie(begin, end, count) = find_adjacent_children(t, child);
                
                // 2.2 - Check if we need to split or only to redistribute
                if(count >= split_limit(t))
                    t.insert_child(end++);
                
                // 2.3 - Redistribute
                redistribute(t, begin, end, count);
                
                // 2.4 - Search again where to insert the bit
                tie(child, new_index) = t.find_insert_point(index);
            }
            
            // 3 - Get the ref to the child into which we're going to recurse,
            //     Note that we need to get the ref before incrementing the
            //     counters in the parent, for consistency
            subtree_ref child_ref = t.child(child);
            
            // 4 - Update counters
            t.sizes(child, degree) += 1;
            t.ranks(child, degree) += bit;
            
            // 5 - Insert recursively
            return insert(child_ref, new_index, bit);
        }
        
        // Utility functions for insert()
        
        // Find the group of children adjacent to 'child',
        // with the maximum number of free slots (bits or keys, it depends).
        // It returns a tuple with:
        //
        // - The begin and the end of the interval selected interval of children
        // - The count of slots contained in total in the found leaves
        //   I repeat: the number of slots, not the number of free slots
        template<size_t W, allocation_policy_t AP>
        std::tuple<size_t, size_t, size_t>
        bt_impl<W, AP>::find_adjacent_children(subtree_const_ref t,
                                               size_t child)
        {
            const bool is_leaf = t.child(child).is_leaf();
            const size_t max_count = is_leaf ? leaf_bits : (degree + 1);
            const auto count = [&](size_t i) {
                return t.pointers(i) == 0 ? max_count :
                       is_leaf            ? leaf_bits - t.child(i).size() :
                                            (degree + 1) - t.child(i).nchildren();
            };
            
            size_t begin = child >= buffer ? child - buffer + 1 : 0;
            size_t end = min(begin + buffer, degree + 1);
            
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
            return std::make_tuple( window.first, window.second, total );
        }
        
        //
        // This function clears the counters relative to children
        // in the range [begin, end), as if the respective subtrees were empty.
        // Pointers and subtrees are not touched.
        // Note that at the end of this function, the subtree_ref is in an
        // inconsistent state regarding its size, so we have to be careful to
        // not call functions such as nchildren() and is_full().
        // This function is used only in redistribute_bits() and
        // redistribute_keys(), to prepare the redistribution. In these
        // functions we already know how many children we want to iterate on,
        // so we don't need nchildren()
        //
        template<size_t W, allocation_policy_t AP>
        void bt_impl<W, AP>::clear_children_counters(subtree_ref t,
                                                     size_t begin,
                                                     size_t end) const
        {
            size_t keys_end = min(end, degree);
            size_t last_size = end < degree ? t.sizes(end - 1)   : size;
            size_t last_rank = end < degree ? t.ranks(end - 1)   : rank;
            size_t prev_size = begin > 0    ? t.sizes(begin - 1) : 0;
            size_t prev_rank = begin > 0    ? t.ranks(begin - 1) : 0;
            
            assert(last_size >= prev_size);
            assert(last_rank >= prev_rank);
            
            t.sizes(begin, keys_end)   = prev_size;
            t.ranks(begin, keys_end)   = prev_rank;
            
            t.sizes(keys_end, degree) -= last_size - prev_size;
            t.ranks(keys_end, degree) -= last_rank - prev_rank;
        }
        
        template<size_t W, allocation_policy_t AP>
        size_t bt_impl<W, AP>::split_limit(subtree_ref t)
        {
            return t.height() == 1 ? buffer * (leaf_bits - buffer)
                                   : buffer * (buffer + 1) ;
        }
        
        template<size_t W, allocation_policy_t AP>
        void bt_impl<W, AP>::redistribute(subtree_ref t,
                                          size_t begin, size_t end,
                                          size_t count)
        {
            if(t.height() == 1)
                redistribute_bits(t, begin, end, count);
            else
                redistribute_keys(t, begin, end, count);
        }
        
        // FIXME: rewrite using a temporary space of two words instead
        //        of the whole buffer, thus avoiding the dynamic allocation
        template<size_t W, allocation_policy_t AP>
        void bt_impl<W, AP>::redistribute_bits(subtree_ref t,
                                               size_t begin, size_t end,
                                               size_t count)
        {
            size_t b = end - begin; // Number of children to use
            size_t bits_per_leaf = count / b; // Average number of bits per leaf
            size_t rem           = count % b; // Remainder
            
            assert(b == buffer || b == buffer + 1);
            
            // Here we use the existing abstraction of packed_view
            // to accumulate all the bits into a temporary buffer, and
            // subsequently redistribute them to the leaves
            bitview<std::vector> bits(count);
            
            for(size_t i = begin, p = 0; i < end; ++i) {
                if(t.pointers(i) != 0) {
                    size_t step = t.child(i).size();
                    leaf_reference leaf = t.child(i).leaf();
                    bits(p, p + step) = leaf(0, leaf.size());
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
                
                // Take the bits out of the buffer, put them back into the leaf
                leaf_reference leaf = t.child(i).leaf();
                leaf.clear();
                leaf(0, leaf.size()) = bits(p, p + n);
                
                // Increment the counters
                t.sizes(i, degree) += n;
                t.ranks(i, degree) += leaf.popcount();
                
                // Count of the copied bits
                p += n;
                count -= n;
            }
            
            assert(count == 0);
            // This is debug code
            // It's important to stay in shape and not get too fat
            if(size >= (buffer * (leaf_bits - buffer))) {
                for(size_t k = begin; k < end; ++k) {
                    size_t minsize = (buffer *
                                      (leaf_bits - buffer)) /
                                     (buffer + 1);
                    size_t childsize = t.child(k).size();
                    assert(childsize >= minsize);
                    unused(minsize, childsize);
                }
            }
        }
        
        // FIXME: rewrite using a temporary space of two words instead
        //        of the whole buffer, thus avoiding the dynamic allocation
        template<size_t W, allocation_policy_t AP>
        void bt_impl<W, AP>::redistribute_keys(subtree_ref t,
                                               size_t begin, size_t end,
                                               size_t count)
        {
            size_t b = end - begin;
            size_t keys_per_node = count / b;
            size_t rem           = count % b;
            
            assert(b == buffer || b == buffer + 1);
            
            struct pointer {
                size_t size;
                size_t rank;
                size_t ptr;
            };
            
            std::vector<pointer> ptrs;
            ptrs.reserve(buffer * (degree + 1));
            
            for(size_t i = begin; i != end; ++i) {
                if(t.pointers(i) != 0) {
                    for(size_t c = 0; c < t.child(i).nchildren(); ++c) {
                        ptrs.push_back({ t.child(i).child(c).size(),
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
                    size_t s = ptrs[p + j].size;
                    size_t r = ptrs[p + j].rank;
                    
                    t.child(i).pointers(j) = ptrs[p + j].ptr;
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
    
    } // namespace internal
    
    /*
     * Implementation of bitvector's interface,
     * that delegates everything to the private implementation
     */
    template<size_t W, allocation_policy_t AP>
    inline
    bitvector_t<W, AP>::bitvector_t(size_t capacity, size_t node_width)
        : _impl(new internal::bt_impl<W, AP>(capacity, node_width)) { }
    
    template<size_t W, allocation_policy_t AP>
    inline
    bitvector_t<W, AP>::bitvector_t(bitvector_t const&other)
        : _impl(other.valid() ? new internal::bt_impl<W, AP>(*other._impl)
                              : nullptr) { }
    
    template<size_t W, allocation_policy_t AP>
    inline
    bitvector_t<W, AP> &bitvector_t<W, AP>::operator=(bitvector_t const&other) {
        if(!valid()) {
            if(other.valid())
                _impl = new internal::bt_impl<W, AP>(*other._impl);
        } else {
            if(other.valid())
                *_impl = *other._impl;
            else
                _impl = nullptr;
        }
        
        return *this;
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    size_t bitvector_t<W, AP>::size() const {
        return valid() ? _impl->size : 0;
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    size_t bitvector_t<W, AP>::capacity() const {
        return valid() ? _impl->capacity : 0;
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    bool bitvector_t<W, AP>::valid() const {
        return bool(_impl);
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    bool bitvector_t<W, AP>::empty() const {
        return !valid() || size() == 0;
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    bool bitvector_t<W, AP>::full() const {
        return valid() && size() == capacity();
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    bool bitvector_t<W, AP>::access(size_t index) const {
        assert(valid() && "Can't access an uninitialized vector");
        assert(index < size() && "Index out of bounds");
        return _impl->access(_impl->root(), index);
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    size_t bitvector_t<W, AP>::rank(size_t index, bool bit) const
    {
        assert(valid() && "Can't access an uninitialized vector");
        size_t rank = _impl->getrank(_impl->root(), index, 0);

        if(!bit)
            rank = index - rank;
            
        return rank;
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    void bitvector_t<W, AP>::set(size_t index, bool bit) {
        assert(valid() && "Can't access an uninitialized vector");
        return _impl->set(_impl->root(), index, bit);
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    void bitvector_t<W, AP>::insert(size_t index, bool bit) {
        assert(valid() && "Can't access an uninitialized vector");
        _impl->insert(_impl->root(), index, bit);
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    auto bitvector_t<W, AP>::operator[](size_t index) -> reference {
        assert(index < size() && "Index out of bounds");
        return { *this, index };
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    auto bitvector_t<W, AP>::operator[](size_t index) const -> const_reference {
        assert(index < size() && "Index out of bounds");
        return { *this, index };
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    void bitvector_t<W, AP>::push_back(bool bit) {
        assert(valid() && "Can't access an uninitialized vector");
               
        insert(size(), bit);
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    void bitvector_t<W, AP>::push_front(bool bit) {
        assert(valid() && "Can't access an uninitialized vector");
        insert(0, bit);
    }
    
    /*
     * Reference types for the access operators
     */
    template<size_t W, allocation_policy_t AP>
    class bitvector_t<W, AP>::const_reference
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
    
    template<size_t W, allocation_policy_t AP>
    class bitvector_t<W, AP>::reference
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
    template<size_t W, allocation_policy_t AP>
    void bitvector_t<W, AP>::test(std::ostream &stream, size_t N, size_t Wn,
                                  bool random,
                                  bool testrank, bool dumpinfo, bool dumpnode,
                                  bool dumpcontents)
    {
        using std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;
        using std::chrono::duration;
        
        bitvector_t v(N, Wn);
        
        if(dumpinfo)
            stream << v << "\n";
        
        assert(v.empty());
        
        size_t nbits = N;
        
        std::mt19937 engine(42);
        std::vector<std::pair<size_t, bool>> indexes;
        bool b = true;
        for(size_t i = 0; i < nbits; ++i) {
            std::uniform_int_distribution<size_t> dist(0, i);
            indexes.emplace_back(random ? dist(engine) : i, b = !b);
        }
        
        auto t1 = high_resolution_clock::now();
        for(size_t i = 0; i < nbits - 1; ++i)
            v.insert(indexes.at(i).first, indexes.at(i).second);
        v.insert(indexes.at(nbits - 1).first, indexes.at(nbits - 1).second);
        auto t2 = high_resolution_clock::now();
        
        assert(v.full());
        
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
        
        if(testrank || dumpcontents) {
            size_t ranki = nbits/2;
            size_t rank = 0;
            size_t totalrank = 0;
            for(size_t i = 0; i < nbits; ++i) {
                bool b = v.access(i);
                if(b) {
                    if(i < ranki)
                        ++rank;
                    ++totalrank;
                }
                
                if (dumpcontents) {
                    if(i && i % 8 == 0)
                        stream << " ";
                    if(i && i % 64 == 0)
                        stream << "\n";
                    stream << b;
                }
            }
            size_t r1 = v.rank(ranki);
            size_t r2 = v.rank(nbits);
            if(r1 != rank)
                stream << "FAIL: rank should be " << rank << ",\n"
                       << "               found " << r1 << "\n";
            
            if(r2 != totalrank)
                stream << "FAIL: total rank should be " << totalrank << ",\n"
                       << "                     found " << r2 << "\n";
        }
        
        if(dumpcontents)
            stream << "\n\n";
        
        
        double total = duration_cast<duration<double, std::ratio<1>>>(t2 - t1).count();
        stream << "Inserted " << nbits << " bits (Wn = " << Wn << ") in "
               << total << "s\n";
        stream << "Used " << v.memory() << " bits of memory\n";
        stream << "Height: " << v.info().height << "\n";
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    auto bitvector_t<W, AP>::info() const -> info_t
    {
        assert(valid() && "Can't access an uninitialized vector");
        return { _impl->capacity,
                 _impl->size,
                 _impl->height,
                 _impl->node_width,
                 _impl->counter_width,
                 _impl->pointer_width,
                 _impl->degree,
                 _impl->buffer,
                 _impl->sizes.size() / std::max(_impl->degree, size_t(1)),
                 _impl->leaves.size()
        };
    }
    
    template<size_t W, allocation_policy_t AP>
    inline
    size_t bitvector_t<W, AP>::memory() const
    {
        assert(valid() && "Can't access an uninitialized vector");
        constexpr size_t nodes_word_size =
                         internal::bitsize<internal::bitview_base::word_type>();
        
        size_t m = sizeof(internal::bt_impl<W, AP>) * 8;
        m += _impl->sizes.container().size() * nodes_word_size;
        m += _impl->ranks.container().size() * nodes_word_size;
        m += _impl->pointers.container().size() * nodes_word_size;
        m += _impl->leaves.size() * internal::bt_impl<W, AP>::leaf_bits;
        
        return m;
    }
    
    template<size_t W, allocation_policy_t AP>
    std::ostream &operator<<(std::ostream &s, bitvector_t<W, AP> const&v) {
        assert(v.valid() && "Can't access an uninitialized vector");
        
        s << "Word width         = " << v._impl->node_width << " bits\n"
          << "Capacity           = " << v._impl->capacity << " bits\n"
          << "Size counter width = " << v._impl->counter_width << " bits\n"
          << "Pointers width     = " << v._impl->pointer_width << " bits\n"
          << "Degree             = " << v._impl->degree << "\n"
          << "b                  = " << v._impl->buffer << "\n"
          << "Number of nodes    = " << v._impl->nodes_count << "\n"
          << "Number of leaves   = " << v._impl->leaves_count << "\n";
        return s;
    }
}
