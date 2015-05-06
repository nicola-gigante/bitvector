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

#ifndef PACKED_BITVECTOR_VIEW_REFERENCE_H
#define PACKED_BITVECTOR_VIEW_REFERENCE_H

namespace bv {
    namespace internal {
        
        // Utility trait used below
        // Whatever the argument, returns void
        template<typename T1, typename T2>
        struct const_trait {
            using type = T2;
        };
        
        template<typename T1, typename T2>
        using const_t = typename const_trait<T1, T2>::type;
        
        template<typename>
        class range_reference;
        
        template<typename Container>
        class const_range_reference
        {
            friend Container;
            template<typename C>
            friend class range_reference;
            
            using word_type = typename Container::word_type;
            
            const_range_reference(Container const&v, size_t begin, size_t end)
                : _v(v), _begin(begin), _end(end) { }
            
        public:
            
            const_range_reference(const_range_reference const&) = default;
            const_range_reference &operator=(const_range_reference const&) = delete;
            
            explicit operator word_type() const {
                size_t capped = std::min(_end,
                                         _begin + _v.elements_per_word());
                
                return _v.get(_begin, capped);
            }
            
            friend std::string to_binary(const_range_reference const&ref,
                                         size_t sep = 8, char ssep = ' ')
            {
                return ref._v.to_binary(ref._begin, ref._end, sep, ssep);
            }
            
        private:
            Container const&_v;
            size_t _begin;
            size_t _end;
        };
        
        template<typename C>
        class range_reference
        {
            friend C;
            
            using word_type = typename C::word_type;
            
            range_reference(C &v, size_t begin, size_t end)
                : _v(v), _begin(begin), _end(end) { }
            
        public:
            range_reference(range_reference const&) = default;
            
            operator const_range_reference<C>() const {
                return { _v, _begin, _end };
            }
            
            range_reference const&operator=(word_type value) const
            {
                _v.set(_begin, _end, value);
                
                return *this;
            }
            
            range_reference const&operator=(range_reference const&ref) const
            {
                operator=(const_range_reference<C>(ref));
                
                return *this;
            }
            
            template<typename C1>
            range_reference const&
            operator=(const_range_reference<C1> const&ref) const
            {
                _v.copy(ref._v,
                        ref._begin, ref._end,
                            _begin,     _end);
                
                return *this;
            }
            
            template<typename C1>
            range_reference const&operator=(range_reference<C1> const&ref) const
            {
                operator=(const_range_reference<C1>(ref));
                
                return *this;
            }
            
            /*
             * These two operators are available only if the underlying class
             * implements increment() and decrement()
             */
            template<typename T = void,
                     typename =
            decltype(std::declval<const_t<T, C>>().increment(size_t(),
                                                             size_t(),
                                                             size_t()))>
            range_reference const&operator+=(size_t n) const
            {
                _v.increment(_begin, _end, n);
                
                return *this;
            }
            
            range_reference const&operator-=(size_t n) const
            {
                _v.decrement(_begin, _end, n);
                
                return *this;
            }
            
            explicit operator word_type() const {
                return word_type(const_range_reference<C>(*this));
            }
            
            friend std::string to_binary(range_reference const&ref,
                                         size_t sep = 8, char ssep = ' ')
            {
                return to_binary(const_range_reference<C>(ref), sep, ssep);
            }
            
        private:
            C &_v;
            size_t _begin;
            size_t _end;
        };
        
        template<typename>
        class item_reference;
        
        template<typename Container>
        class const_item_reference
        {
            friend Container;
            friend item_reference<Container>;
            
            using value_type = typename Container::value_type;
            
            const_item_reference(Container const&v, size_t index)
                : _v(v), _index(index) { }
            
        public:
            const_item_reference(const_item_reference const&) = default;
            
            value_type value() const {
                return _v.get(_index);
            }
            
            operator value_type() const {
                return value();
            }
            
        private:
            Container const&_v;
            size_t _index;
        };
        
        template<typename Container>
        class item_reference
        {
            friend Container;
            
            using value_type = typename Container::value_type;
            
            item_reference(Container &v, size_t index)
                : _v(v), _index(index) { }
            
        public:
            item_reference(item_reference const&) = default;
            
            operator const_item_reference<Container>() const {
                return { _v, _index };
            }
            
            value_type value() const {
                return const_item_reference<Container>(*this).value();
            }
            
            operator value_type() const {
                return value();
            }
            
            item_reference const&operator=(value_type v) const {
                _v.set(_index, v);
                
                return *this;
            }
            
            item_reference const&operator=(item_reference r) const {
                _v.set(_index, r.value());
                
                return *this;
            }
            
            friend
            void swap(item_reference<Container> r1,
                      item_reference<Container> r2)
            {
                using std::swap;
                
                value_type v = r1;
                r1 = r2;
                r2 = v;
            }
            
        private:
            Container &_v;
            size_t _index;
        };
    }
}

#endif
