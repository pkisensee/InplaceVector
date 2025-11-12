# inplace_vector

Inplace_vector wraps a contiguous chunk of memory with a maximum size, introduced in C++26. Think of it as a std::vector that lives on the stack rather than the heap.
The advantage of inplace_vector is that it performs no memory allocations (unless the object T itself allocates), and unlike std::array, it only constructs elements as needed, so it's more efficient.
The disadvantage is that inplace_vector always takes up a fixed amount of stack memory that must be defined at compile time. It's useful when you're storing a relatively small amount of objects that will never exceed a given size.

Since not all implementations support this class yet, here's a version that you can grab and use even if you don't have a C++26 compatible implementation.
For simplicity, this implementation doesn't specialize to have zero storage when Capacity is zero. That's an edge case I've haven't needed to worry about.

More details at [cppreference](https://en.cppreference.com/w/cpp/container/inplace_vector.html)

Features:
* Header-only implementation; no dependencies other than standard C++ headers
* constexpr enabled
* comparison operations
* range support

Doesn't support:
* zero storage when Capacity is zero
 
Requirements:
* C++17 and up
