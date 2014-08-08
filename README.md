The bitvector class
===============
A C++ container-like data structure for storing vector of bits with fast
appending on both sides and fast insertion in the middle, all in succinct space.

The bitvector is implemented as a packed B+-tree. For a full explaination of the
data structure, see the coming soon paper from Prezza, Policriti et al. (I'll link it here when ready).

Usage
======
The data structure couldn't be more easy to use. It features a container-like interface
so you can use it more or less like a vector. The maximum capacity has to be set in advance,
from the constructor. An optional parameter let you tune for performance the width (in bits)
of the internal nodes of the tree.

```cpp
bitvector v(100000);

for(bool b : { true, false, false, true, true })
    v.push_back(b); // Appending

v.insert(3, false); // Insert in the middle

v[1] = true; // Sets a specific bit

std::cout << v[0] << "\n"; // Access the bits
```

The class is copyable and movable. Moves are very fast so you can return bitvectors by value if you want.

Requirements
===========
The code is written in standard-compliante C++11. Until now I've tested it only with clang on OS X,
but testing with gcc on Linux is coming soon. There aren't any external dependencies.

Performance
==========
Benchmarks coming soon, but it's promising.
As said, space is provably succinct, so as the capacity grows, the space overhead goes to 0.
