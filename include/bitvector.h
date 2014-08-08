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
    class bitvector
    {
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
        bitvector(size_t N, size_t W = 256);
        // Destructor is defaulted but must be out-of-line to use
        // a unique_ptr of an incomplete type
        ~bitvector();

        bitvector(bitvector const&);
        bitvector(bitvector &&) = default;
        
        bitvector &operator=(bitvector const&);
        bitvector &operator=(bitvector &&) = default;
        
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
        static void test(std::ostream &stream, size_t N, size_t W,
                         bool dumpinfo, bool dumpnode, bool dumpcontents);
        friend std::ostream &operator<<(std::ostream &s, bitvector const&v);
        
    private:
        std::unique_ptr<struct bt_impl> _impl;
    };
    
    /************************************************************************/
    
    /*
     * Reference types for the access operators
     */
    class bitvector::const_reference
    {
        friend class bitvector;
        
        bitvector const&_v;
        size_t _index;
        
        const_reference(bitvector const&v, size_t index)
            : _v(v), _index(index) { }
    public:
        const_reference(const_reference const&) = default;
        
        operator bool() const {
            return _v.access(_index);
        }
    };
    
    class bitvector::reference
    {
        friend class bitvector;
        
        bitvector &_v;
        size_t _index;
        
        reference(bitvector &v, size_t index)
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
     * Inline implementation of some trivial member function that really should
     * be inlined (but written here out of the class declaration for aesthetics)
     */
    inline
    bitvector::reference
    bitvector::operator[](size_t index) {
        assert(index < size());
        return { *this, index };
    }
    
    inline
    bitvector::const_reference
    bitvector::operator[](size_t index) const {
        assert(index < size());
        return { *this, index };
    }
    
    inline
    void bitvector::push_back(bool bit) {
        insert(size(), bit);
    }
    
    inline
    void bitvector::push_front(bool bit) {
        insert(0, bit);
    }
}



#endif // BITVECTOR_H
