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
#define PACKED_BITVECTOR_H

#include "internal/bits.h"
#include "packed_view.h"

#include <memory>
#include <ostream>

namespace bv
{
    /*
     * === Bitvector class ===
     *
     * This class implement a vector of bits with fast append at both sides and
     * insertion in the middle in succinct space.
     *
     * See README.md for a brief explaination of the class usage
     *
     */
    enum allocation_policy_t {
        alloc_on_demand,
        alloc_immediatly
    };
    
    namespace internal {
        template<size_t, allocation_policy_t>
        struct bt_impl;
    }
    
    template<size_t W, allocation_policy_t AllocPolicy = alloc_on_demand>
    class bitvector_t
    {
        static_assert(W % internal::bitsize<internal::bitview_base::word_type>() == 0,
                      "You must choose a number of bits that is multiple "
                      "of the word size");
    public:
        /*
         * Types and typedefs
         */
        using value_type = bool;
        class reference;
        class const_reference;
        
        /*
         * Constructors, copies and moves...
         */
        bitvector_t() = default;
        bitvector_t(size_t N, size_t Wn = 256);
        ~bitvector_t() = default;

        bitvector_t(bitvector_t const&);
        bitvector_t(bitvector_t &&) = default;
        
        bitvector_t &operator=(bitvector_t const&);
        bitvector_t &operator=(bitvector_t &&) = default;
        
        /*
         * Accessors
         */
        size_t size() const;
        size_t capacity() const;
        bool valid() const;
        bool empty() const;
        bool full() const;
        
        /*
         * Data operations
         */
        bool access(size_t index) const;
        size_t rank(size_t index, bool bit = true) const;
        void set(size_t index, bool bit);
        void insert(size_t index, bool bit);
        void push_back(bool bit);
        void push_front(bool bit);
        
        /*
         * Operators for easy access
         */
        reference       operator[](size_t index);
        const_reference operator[](size_t index) const;
        
        // Debugging
        struct info_t {
            const size_t capacity;
            const size_t size;
            const size_t height;
            const size_t node_width;
            const size_t counter_width;
            const size_t pointer_width;
            const size_t degree;
            const size_t buffer;
            const size_t nodes;
            const size_t leaves;
        };
        info_t info() const;
        size_t memory() const;
        static void test(std::ostream &stream, size_t N, size_t Wn,
                         bool random,
                         bool testrank, bool dumpinfo,
                         bool dumpnode, bool dumpcontents);
        template<size_t Z, allocation_policy_t AP>
        friend std::ostream &operator<<(std::ostream &s,
                                        bitvector_t<Z, AP> const&v);
        
    private:
        std::unique_ptr<internal::bt_impl<W, AllocPolicy>> _impl;
    };
    
    using bitvector = bitvector_t<512, alloc_on_demand>;
}

#include "internal/bitvector.hpp"

#endif // PACKED_BITVECTOR_H
