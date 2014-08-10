The bitvector class
===============
A C++ container-like data structure for storing vector of bits with fast
appending on both sides and fast insertion in the middle, all in succinct space.

The bitvector is implemented as a packed B+-tree. For a full explaination of the
data structure, and how we use it, see the coming soon paper from
Prezza, Policriti et al. (I'll link it here when ready).

Requirements
===========
The code is written in standard-compliant C++11.
I've tested the code under clang 3.4 and g++ 4.9, and I'm quite sure it's 100%
portable. More testing is always appreciated.

Usage
======
The library is header-only, so you don't need to compile anything in order to
use it, just #include "bitvector.h" and use the class.

The data structure couldn't be more easy to use. It features a container-like
interface so you can use it more or less like a vector. The maximum capacity has
to be set in advance, from the constructor. An optional second parameter to the
constructor let you change the width (in bits) of the internal nodes of the 
tree, to tune performance (increasing nodes width makes the height decrease,
but the cost of handling larger nodes balances).

```cpp
bitvector v(100000);

for(bool b : { true, false, false, true, true })
    v.push_back(b); // Appending

v.insert(3, false); // Insert in the middle

v[1] = true; // Sets a specific bit

std::cout << v[0] << "\n"; // Access the bits
```

The class is copyable and movable. Since 
```sizeof(bitvector) == sizeof(void *)```, moves are very fast so you can return
bitvectors by value if you want.

Removal operations are not implemented since the are completely non-trivial and
are not needed by our main application of this data structure.

bitvector is actually a typedef for bitvector_t<W>, where W is the width in bits
of the leaves of the tree, filled with a tuned default value.
The leaves width can only be set at compile-time with this template parameter.
If the performance with the default value for W doesn't fit your
machine/architecture/compiler, use bitvector_t and choose the most suitable
value.

Auxiliary data-structures
==========================
As a side benefit, two classes used inside bitvector could be useful on their
own:
* ```bitview<Container>``` is an adaptor around any container with a 
  ```operator[]```. It instiantiate the container with elements of type uint64_t
  and presents a uniform sequence of bits. Then you have a couple of functions
  to set, get and copy around subsequences.
* ```packed_view<Container>``` is built around ```bitview``` and is an adaptor
  that presents a sequence of "fields" of fixed width of arbitrary length. For
  example you can turn a vector of uint64_t into a vector of integer fields of
  18bits each. With difference from ```bitview```, this class provides a nice
  interface with operator[] to access single fields and operator()(begin, end),
  to access ranges of fields, where each operations (e.g. increment with +=)
  is applied to each field in the range.
  
I haven't take the time yet to fully document these classes because their 
inteface, although as generic as possible, is tied with how I use them in 
```bitvector``` (for example, packed_view doesn't have iterators).
Nonetheless, I suppose they could be useful for other purposes.

Testings
=========
A complete test suite is still needed. In the meantime, I have some tests of
correctness for internal components and for the data structures itself in
the test/main.cpp file, with a Makefile to build the test. To aid the
comparation between different compilers, the makefile outputs a binary suffixed
with the name of the compiler used, if any has been specified explicitly, so if
you do:

$ CXX=clang++ make
$ CXX=g++ make

You should end up with two binaries, test-clang++ and test-g++.

Performance
==========
Benchmarks coming soon, but it's promising.
As said, space is provably succinct, so as the capacity grows, the space
overhead goes to 0. While time complexity are theoretically good at the
expense of a complex implementation, the succinct space is the main theoretical
reason of existence of this structure.

TODO
=====
Some work has still to be done, see TODO.txt for details.
In a few words, it still lacks:
* The ability to incrementally allocate the memory used for nodes and leaves
  instead of allocating everything at once at the beginning (mainly it means
  switching from std::vector to std::deque as the underlying storage for 
  the nodes. Coming soon.)
* Performance tuning and profiling
* A serious test suite
* More operations on the data structure. For example, the insertion of words
  instead of single bits in one shot is feasible, but not completely trivial.
  Iteration could also be done relatively easily, but needs modifications
  of the data layout. Removal operations are difficult.

