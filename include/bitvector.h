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
    struct bt_impl;
    class bitvector
    {
    public:
        /*
         * Constructors, copies and moves...
         */
        bitvector(size_t N, size_t W = 256);
        // Destructor must be out-of-line to use
        // an unique_ptr of an incomplete type
        ~bitvector();

        bitvector(bitvector const&);
        bitvector(bitvector &&) = default;
        
        bitvector &operator=(bitvector const&);
        bitvector &operator=(bitvector &&) = default;
        
        /*
         * Accessors
         */
        bool empty() const;
        bool full() const;
        
        /*
         * Data operations
         */
        bool access(size_t index) const;
        void insert(size_t index, bool bit);
        
        // Debugging
        static void test(std::ostream &stream);
        friend std::ostream &operator<<(std::ostream &s, bitvector const&v);
        
    private:
        std::unique_ptr<bt_impl> _impl;
    };
}

#endif // BITVECTOR_H
