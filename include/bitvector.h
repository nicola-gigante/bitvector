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

#include <cstddef>
#include <cstdint>

/*
 * Generic configurable-size word type
 */
template<size_t W>
struct word_t { };

template<>
struct word_t<8> {
    using type = uint8_t;
};

template<>
struct word_t<16> {
    using type = uint16_t;
};

template<>
struct word_t<32> {
    using type = uint32_t;
};

template<>
struct word_t<64> {
    using type = uint64_t;
};

// FIXME: test __uint128_t availability
template<>
struct word_t<128> {
    using type = __uint128_t;
};


template<size_t W>
using word = typename word_t<W>::type;


#endif // BITVECTOR_H
