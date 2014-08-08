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
     * insertion in the middle in succint space.
     *
     * The interface of the class is as container-like as possible.
     * The constructor takes the maximum number of bits that will be contained
     * in the bitvector, and an optional value specifying the width of the
     * nodes of the packed B+-tree (you can tune the value for performance)
     *
     * The use is straightforward:
     *
     *     bitvector v(100000);
     *     
     *     for(bool b : { true, false, false, true, true })
     *         v.push_back(b); // Appending
     *     
     *     v.insert(3, false); // Insert in the middle
     *
     *     v[1] = true; // Sets a specific bit
     *
     *     std::cout << v[0] << "\n"; // Access the bits
     *
     * The class is copyable and movable. Moves are very fast so you can
     * for example return a fresh bitvector by value. 
     *
     */
    template<size_t W>
    struct bt_impl;
    
    template<size_t W>
    class bitvector_t
    {
        static_assert(W % bitsize<bitview_value_type>() == 0,
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
        bool empty() const;
        bool full() const;
        
        /*
         * Data operations
         */
        bool access(size_t index) const;
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
            const size_t node_width;
            const size_t counter_width;
            const size_t pointer_width;
            const size_t degree;
            const size_t buffer;
            const size_t nodes;
            const size_t leaves;
        };
        info_t info() const;
        static void test(std::ostream &stream, size_t N, size_t Wn,
                         bool dumpinfo, bool dumpnode, bool dumpcontents);
        template<size_t Z>
        friend std::ostream &operator<<(std::ostream &s, bitvector_t<Z> const&v);
        
    private:
        std::unique_ptr<bt_impl<W>> _impl;
    };
    
    using bitvector = bitvector_t<512>;
}

#include "internal/bitvector.hpp"


#endif // BITVECTOR_H
