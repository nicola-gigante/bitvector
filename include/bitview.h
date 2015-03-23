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

#ifndef PACKED_BITVECTOR_BITVIEW_H
#define PACKED_BITVECTOR_BITVIEW_H

#include "internal/bits.h"
#include "internal/view_reference.h"

#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <limits>
#include <tuple>
#include <algorithm>

namespace bv
{
    namespace internal {
        class bitview_base {
        public:
            using word_type = uint64_t;
        };
        
        template<template<typename ...> class Container>
        class bitview : public bitview_base
        {
        public:
            using bitview_base::word_type;
            using value_type = bool;

            static constexpr size_t W = bitsize<word_type>();
            
            using range_reference       = internal::range_reference<bitview>;
            using const_range_reference = internal::const_range_reference<bitview>;
            using bit_reference         = internal::item_reference<bitview>;
            using const_bit_reference   = internal::const_item_reference<bitview>;
            
            using container_type = Container<word_type>;
            
            bitview() = default;
            
            template<typename Size = size_t,
                     REQUIRES(std::is_constructible<container_type, Size>::value)>
            bitview(size_t size)
                : _container(required_container_size(size)) { }
            
            bitview(bitview const&) = default;
            bitview(bitview &&) = default;
            bitview &operator=(bitview const&) = default;
            bitview &operator=(bitview &&) = default;
            
            container_type const&container() const { return _container; }
            container_type      &container()       { return _container; }
            
            // Number of bits handled by the view
            size_t size() const { return _container.size() * W; }
            
            // A default-initialized bitview is empty
            bool empty() const { return size() == 0; }
            
            // Change the size of the view, possibly changing the size of the
            // underlying container as well
            template<typename C = container_type,
                     typename = decltype(std::declval<C>().resize(size_t()))>
            void resize(size_t size) {
                return _container.resize(required_container_size(size));
            }
            
            size_t popcount(size_t begin, size_t end) const;
            size_t popcount() const;
            
            void clear();
            
            void insert(size_t begin, size_t end, word_type value);
            void insert(size_t index, bool bit);
            
            /*
             * Interface to the reference wrapper.
             * It is recommended to use the high-level interface instead
             */
            size_t elements_per_word() const { return W; }
            
            word_type get(size_t begin, size_t end) const;
            void set(size_t begin, size_t end, word_type value);
            
            bool get(size_t index) const;
            void set(size_t index, bool bit);
            
            void copy(bitview const&src,
                      size_t src_begin, size_t src_end,
                      size_t dest_begin, size_t dest_end);
            
            template<template<typename ...> class C>
            void copy(bitview<C> const&src,
                      size_t src_begin, size_t src_end,
                      size_t dest_begin, size_t dest_end);
            
            std::string to_binary(size_t begin, size_t end,
                                  size_t sep, char ssep) const;
            
            /*
             * High-level access interface to the view's contents
             */
            range_reference operator()(size_t begin, size_t end) {
                return { *this, begin, end };
            }
            
            const_range_reference operator()(size_t begin, size_t end) const {
                return { *this, begin, end };
            }
            
            bit_reference operator[](size_t index) {
                assert(index < size());
                return { *this, index };
            }
            
            const_bit_reference operator[](size_t index) const {
                assert(index < size());
                return { *this, index };
            }
            
        private:
            static size_t required_container_size(size_t size) {
                return ceildiv(size, W);
            }
            
            struct range_location_t {
                size_t index;
                size_t lbegin;
                size_t llen;
                size_t hlen;
            };
            
            range_location_t locate(size_t begin, size_t end) const;
            
            template<template<typename ...> class C>
            void copy_forward(bitview<C> const&src,
                              size_t src_begin, size_t src_end,
                              size_t dest_begin, size_t dest_end);
            
            template<template<typename ...> class C>
            void copy_backward(bitview<C> const&src,
                               size_t src_begin, size_t src_end,
                               size_t dest_begin, size_t dest_end);
        private:
            container_type _container;
        };
        
        template<size_t Bits>
        struct bitarray_t {
            template<typename T>
            using array = std::array<T, Bits / bitsize<bitview_base::word_type>()>;
        };
        
        template<size_t Bits>
        using bitarray = bitview<bitarray_t<Bits>:: template array>;
        
        template<template<typename ...> class C>
        std::string to_binary(bitview<C> const&v,
                              size_t sep = 8, char ssep = ' ') {
            return v.to_binary(0, v.size(), sep, ssep);
        }
        
        /*
         * Class implementation
         */
        template<template<typename ...> class C>
        typename bitview<C>::range_location_t
        bitview<C>::locate(size_t begin, size_t end) const
        {
            size_t index  = begin / W;
            size_t lbegin = begin % W;
            
            size_t len = end - begin;
            size_t llen = std::min(W - lbegin, len);
            size_t hlen = len - llen;
            
            return { index, lbegin, llen, hlen };
        }
        
        
        /*
         * This very fast and clever code is courtesy of Hans Klünder
         * from this Stack Overflow question:
         *
         * http://stackoverflow.com/questions/27617924
         *
         * Original comments left.
         */
        template < template<typename ...> class C >
        bitview_base::word_type
        bitview<C>::get(size_t begin, size_t end) const
        {
            static constexpr size_t bits = bitsize<word_type>();
            static constexpr size_t mask = ~ size_t(0) / bits * bits;
            
            // everything above has been computed at compile time.
            // Now do some work:
            
            // the index in the container of
            // the lower or higher item needed
            size_t lo_index = (begin          ) / bits;
            size_t hi_index = (end - (end > 0)) / bits;
            
            // we read container[hi_adr] first and possibly delete
            // the highest bits:
            size_t hi_shift = size_t(mask - end) % bits;
            word_type hi_val = _container[hi_index] << hi_shift >> hi_shift;
            
            // if all bits are in the same item,
            // we delete the lower bits and are done:
            size_t lo_shift = begin % bits;
            if ( hi_index <= lo_index )
                return (hi_val >> lo_shift) * (begin < end);
            
            // else we have to read the lower item as well, and combine both
            return ( hi_val << (bits - lo_shift)    |
                     _container[lo_index] >> lo_shift );
            
        }
        
        template<template<typename ...> class Container>
        bool bitview<Container>::get(size_t index) const {
            assert(index < size());
            
            return (_container[index / W] & (word_type(1) << (index % W))) != 0;
        }
        
        template<template<typename ...> class Container>
        size_t bitview<Container>::popcount(size_t begin, size_t end) const
        {
            if(is_empty_range(begin, end))
                return 0;
            
            check_valid_range(begin, end, size());
            
            size_t len = (end - begin);
            size_t rem = len % W;
            
            size_t result = 0;
            for(size_t step, p = begin;
                p < end;
                len -= step, p += step)
            {
                step = len < W ? rem : W;
                
                result += ::bv::internal::popcount(get(p, p + step));
            }
            
            return result;
        }
        
        template<template<typename ...> class Container>
        size_t bitview<Container>::popcount() const {
            return popcount(0, size());
        }
        
        template<template<typename ...> class Container>
        void bitview<Container>::set(size_t begin, size_t end, word_type value)
        {
            if(is_empty_range(begin, end))
                return;
            
            size_t len = end - begin;
            
            assert(len <= W);
            check_valid_range(begin, end, size());
            ensure_bitsize(value, len);
            
            range_location_t loc = locate(begin, end);
            
            set_bitfield(_container[loc.index],
                         loc.lbegin, loc.lbegin + loc.llen, value);
            
            if(loc.hlen != 0) {
                word_type bits = bitfield(value, loc.llen, loc.llen + loc.hlen);
                set_bitfield(_container[loc.index + 1], 0, loc.hlen, bits);
            }
        }
        
        template<template<typename ...> class Container>
        void bitview<Container>::set(size_t index, bool bit)
        {
            const word_type mask    = ~(word_type(1) << (index % W));
            const word_type bitmask = word_type(bit) << (index % W);
            
            _container[index / W] = (_container[index / W] & mask) | bitmask;
        }
        
        template<template<typename ...> class Container>
        void bitview<Container>::clear()
        {
            std::fill(_container.begin(), _container.end(), 0);
        }
        
        template<template<typename ...> class Container>
        template<template<typename ...> class C>
        void bitview<Container>::copy_forward(bitview<C> const&srcbv,
                                              size_t src_begin, 
                                              size_t src_end,
                                              size_t dest_begin, 
                                              size_t dest_end)
        {
            assert(src_end - src_begin == dest_end - dest_begin);

            size_t dest_begin_index = dest_begin / W;
            size_t dest_begin_pos   = dest_begin % W;
            size_t dest_end_index   = dest_end   / W;
            size_t dest_end_pos     = dest_end   % W;

            size_t highpart = (dest_begin_index == dest_end_index) * 
                              (W - dest_end_pos);

            size_t len = (W - dest_begin_pos) - highpart;

            word_type v =
                (srcbv.get(src_begin, src_begin + len) << dest_begin_pos) |
                lowbits (_container[dest_begin_index], dest_begin_pos)    |
                highbits(_container[dest_begin_index], highpart);
            _container[dest_begin_index] = v;

            size_t src_pos = src_begin + len;
            for(size_t i = dest_begin_index + 1; 
                i < dest_end_index; 
                ++i, src_pos += W) 
            {
                _container[i] = srcbv.get(src_pos, src_pos + W);
            }

            len = src_end - src_pos;
            if(len > 0) {
                v = srcbv.get(src_pos, src_pos + len) |
                    highbits(_container[dest_end_index], W - len);
                _container[dest_end_index] = v;
            }
        }
        
        template<template<typename ...> class Container>
        template<template<typename ...> class C>
        void bitview<Container>::copy_backward(bitview<C> const&srcbv,
                                               size_t src_begin, 
                                               size_t src_end,
                                               size_t dest_begin,
                                               size_t dest_end)
        {
            assert(src_end - src_begin == dest_end - dest_begin);

            size_t dest_begin_index = dest_begin / W;
            size_t dest_begin_pos   = dest_begin % W;
            size_t dest_end_index   = dest_end   / W;
            size_t dest_end_pos     = dest_end   % W;

            size_t lowpart = 
                dest_begin_pos * (dest_begin_index == dest_end_index);

            size_t lastpart = dest_end_pos - lowpart;

            size_t src_pos = src_end - lastpart;
            _container[dest_end_index] =
                highbits(_container[dest_end_index], W - dest_end_pos) |
                (srcbv.get(src_pos, src_end) << lowpart)               |
                lowbits(_container[dest_end_index], lowpart);

            size_t dest_pos = dest_end - lastpart;

            while(dest_pos - dest_begin >= W) {
                dest_pos -= W;
                src_pos  -= W;

                _container[dest_pos / W] = srcbv.get(src_pos, src_pos + W);
            }

            size_t firstpart = src_pos - src_begin;
            if(firstpart > 0) {
                _container[dest_begin_index] = 
                    lowbits(_container[dest_begin_index], dest_begin_pos) |
                    (srcbv.get(src_begin, src_pos) << dest_begin_pos);
            }
        }
        
        template<template<typename ...> class Container>
        template<template<typename ...> class C>
        void bitview<Container>::copy(bitview<C> const&src,
                                      size_t src_begin, size_t src_end,
                                      size_t dest_begin, size_t dest_end)
        {
            size_t srclen = src_end - src_begin;
            size_t destlen = dest_end - dest_begin;
            
            if(destlen < srclen) {
                src_end = src_begin + destlen;
                srclen  = destlen;
            }
            
            copy_forward(src, src_begin, src_end, 
                         dest_begin, dest_begin + srclen);
        }
        
        template<template<typename ...> class Container>
        void bitview<Container>::copy(bitview const&src,
                                      size_t src_begin, size_t src_end,
                                      size_t dest_begin, size_t dest_end)
        {
            size_t srclen = src_end - src_begin;
            size_t destlen = dest_end - dest_begin;
            
            if(destlen < srclen) {
                src_end = src_begin + destlen;
                srclen  = destlen;
            }
            
            if(this == &src && src_begin < dest_begin)
                copy_backward(src, src_begin, src_end, 
                              dest_begin, dest_begin + srclen);
            else
                copy_forward(src, src_begin, src_end, 
                             dest_begin, dest_begin + srclen);
        }
        
        template<template<typename ...> class Container>
        void bitview<Container>::insert(size_t begin, size_t end,
                                        word_type value)
        {
            if(is_empty_range(begin, end))
                return;
            
            check_valid_range(begin, end, size());
            
            copy(*this, begin, size(), end, size());
            set(begin, end, value);
        }
        
        template<template<typename ...> class Container>
        void bitview<Container>::insert(size_t index, bool bit)
        {
            size_t i = index / W;
            size_t pos = index % W;

            word_type word = _container[i];
            word_type mask = word_type(-1) << pos;

            word_type carry = word >> (W - 1);
            _container[i] = ((word &  mask) << 1)    | 
                             (word & ~mask)          | 
                             (word_type(bit) << pos);

            for(size_t j = i + 1; j < _container.size(); ++j) 
            {
                word = _container[j];
                word_type newcarry = word >> (W - 1);
                
                _container[j] = word << 1 | carry;
                carry = newcarry;
            }
        }
        
        template<template<typename ...> class Container>
        std::string bitview<Container>::to_binary(size_t begin, size_t end,
                                                  size_t sep, char ssep) const
        {
            std::string s;
            
            // It is slooooow. Oh well.. it's debugging output after all...
            for(size_t i = begin, bits = 0; i < end; ++i, ++bits) {
                if(bits && bits % sep == 0)
                    s += ssep;
                s += get(i) ? '1' : '0';
            }
            
            std::reverse(s.begin(), s.end());
            
            return s;
        }
    } // namespace internal
    
    // Public things
    using internal::bitview;
    using internal::bitarray;
} // namespace bv

#endif
