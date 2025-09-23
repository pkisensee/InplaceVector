///////////////////////////////////////////////////////////////////////////////
//
//  inplace_vector.h
//
//  Copyright © Pete Isensee (PKIsensee@msn.com).
//  All rights reserved worldwide.
//
//  Permission to copy, modify, reproduce or redistribute this source code is
//  granted provided the above copyright notice is retained in the resulting 
//  source code.
// 
//  This software is provided "as is" and without any express or implied
//  warranties.
// 
// -----------------------------------------------------------------------------
//
//  Implements C++26 std::inplace_vector 
// 
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <array>
#include <ranges>

// Uncomment the following line to enable exceptions, or define this symbol at build time
// #define PK_ENABLE_EXCEPTIONS 1

#if defined(PK_ENABLE_EXCEPTIONS)
#include <stdexcept>
#define PK_MAY_THROW noexcept(false) // indicates function may throw
#else
#define PK_MAY_THROW noexcept(true) // function will not throw
#endif

namespace // anonymous
{
  // Requirements for object comparisons; see SynthThreeWay
  template <typename T>
  concept BooleanTestableImpl = std::convertible_to<T, bool>;

  template <typename T>
  concept BooleanTestable = BooleanTestableImpl<T>
    && requires( T && a ) {
      { !static_cast<T&&>( a ) } -> BooleanTestableImpl;
  };

  /*
  // Helper converts input iterators into an array
  // Usage: std::array<int, Capacity> arr = MakeArray<InIt, Capacity>( vec.begin(), vec.end() );
  template<typename InIt, size_t Capacity>
  constexpr auto MakeArray( InIt first, InIt last )
  {
    const auto count = std::distance( first, last );
    assert( count <= Capacity );
    using ValType = typename std::iterator_traits<InIt>::value_type;
    std::array<ValType, Capacity> arr;
    std::copy_n( first, count, std::begin( arr ) );
    return arr;
  }

  // Helper converts range into an array
  // Usage: std::array<int, Capacity> arr = MakeArray<Range, Capacity>( rng );
  template<typename Range, size_t Capacity>
  constexpr auto MakeArray( Range&& rng )
    requires std::ranges::sized_range<decltype( rng )>
  {
    const auto count = static_cast<int64_t>( std::ranges::size( rng ) );
    assert( count <= Capacity );
    using ValType = std::iter_value_t<decltype( rng )>;
    std::array<ValType, Capacity> arr;
    std::ranges::copy_n( std::begin( rng ), count, std::begin( arr ) );
    return arr;
  }
  */

}; // namespace anonymous

namespace PKIsensee
{

  template <typename T, size_t Capacity> // stack of objects T; maximum size Capacity
  class inplace_vector
  {
  public:

    using Array = std::array<T, Capacity>;
    using container_type = Array;
    using value_type = typename Array::value_type;
    using pointer = typename Array::pointer;
    using reference = typename Array::reference;
    using const_reference = typename Array::const_reference;
    using iterator = typename Array::iterator;
    using const_iterator = typename Array::const_iterator;
    using reverse_iterator = typename Array::reverse_iterator;
    using const_reverse_iterator = typename Array::const_reverse_iterator;
    using size_type = typename Array::size_type;

    constexpr inplace_vector() noexcept
    {

    }

    constexpr explicit inplace_vector( size_type n )
    {

    }

    constexpr inplace_vector( size_type n, const T& value )
    {

    }

    template <typename InIt>
    constexpr inplace_vector( InIt first, InIt last )
    {
    }

    template <typename Range>
    constexpr inplace_vector( std::from_range_t, Range&& rng )
    {
    }

    constexpr inplace_vector( const inplace_vector& c )
    {
    }

    constexpr inplace_vector( inplace_vector&& c )
      noexcept( Capacity == 0 || std::is_nothrow_move_constructible_v<T> )
    {
    }

    constexpr inplace_vector( std::initializer_list<T> il )
    {
    }

    constexpr ~inplace_vector()
    {
    }

    constexpr inplace_vector& operator=( const inplace_vector& rhs )
    {
      return *this;
    }

    constexpr inplace_vector& operator=( inplace_vector&& rhs )
      noexcept( Capacity == 0 || ( std::is_nothrow_move_assignable_v<T> &&
                            std::is_nothrow_move_constructible_v<T> ) )
    {
      return *this;
    }

    constexpr inplace_vector& operator=( std::initializer_list<T> il )
    {
      return *this;
    }

    template <typename InIt>
    constexpr void assign( InIt first, InIt last )
    {
    }

    template <typename Range>
    constexpr void assign_range( Range&& rng )
    {
    }

    constexpr void assign( size_type n, const T& u )
    {
    }

    constexpr void assign( std::initializer_list<T> il )
    {
    }

    constexpr iterator begin() noexcept
    {
    }

    constexpr const_iterator begin() const noexcept
    {
    }

    constexpr const_iterator cbegin() const noexcept
    {
    }

    constexpr iterator end() noexcept
    {
    }

    constexpr const_iterator end() const noexcept
    {
    }

    constexpr const_iterator cend() const noexcept
    {
    }

    constexpr reverse_iterator rbegin() noexcept
    {
    }

    constexpr const_reverse_iterator rbegin() const noexcept
    {
    }

    constexpr const_reverse_iterator crbegin() const noexcept
    {
    }

    constexpr reverse_iterator rend() noexcept
    {
    }

    constexpr const_reverse_iterator rend() const noexcept
    {
    }

    constexpr const_reverse_iterator crend() const noexcept
    {
    }

    constexpr bool empty() const noexcept
    {
    }

    constexpr size_type size() const noexcept
    {
    }

    static constexpr size_type max_size() noexcept
    {
    }

    static constexpr size_type capacity() noexcept
    {
      return Capacity;
    }

    constexpr void resize( size_type sz )
    {
    }

    constexpr void resize( size_type sz, const T& c )
    {
    }

    static constexpr void reserve( size_type n )
    {
    }

    static constexpr void shrink_to_fit() noexcept
    {
    }

    constexpr const_reference operator[]( size_type i ) const
    {
    }

    constexpr reference operator[]( size_type i )
    {
    }

    constexpr const_reference at( size_type i ) const
    {
    }

    constexpr reference at( size_type i )
    {
    }

    constexpr const_reference front() const
    {
    }

    constexpr reference front()
    {
    }

    constexpr const_reference back() const
    {
    }

    constexpr reference back()
    {
    }

    constexpr const T* data() const noexcept
    {
    }

    constexpr T* data() noexcept
    {
    }

    template <typename... Types>
    constexpr reference emplace( Types&&... values )
    {
    }

    constexpr reference push_back( const T& value )
    {
    }

    constexpr reference push_back( T&& value )
    {
    }

    template <typename Range>
    constexpr void append_range( Range&& rng )
    {
    }

    constexpr void pop_back()
    {
    }

    template <typename... Types>
    constexpr pointer try_emplace_back( Types&&... values )
    {
    }

    constexpr pointer try_push_back( const T& value )
    {
    }

    constexpr pointer try_push_back( T&& value )
    {
    }

    template <typename Range>
    constexpr std::ranges::borrowed_iterator_t<Range> try_append_range( Range&& rng )
    {
    }

    template <typename... Types>
    constexpr reference unchecked_emplace_back( Types&&... values )
    {
    }

    constexpr reference unchecked_push_back( const T& value )
    {
    }

    constexpr reference unchecked_push_back( T&& value )
    {
    }

    template <typename... Types>
    constexpr iterator emplace( const_iterator position, Types&&... values )
    {
    }

    constexpr iterator insert( const_iterator position, const T& value )
    {
    }

    constexpr iterator insert( const_iterator position, T&& value )
    {
    }

    constexpr iterator insert( const_iterator position, size_type n, const T& value )
    {
    }

    template <class InIt>
    constexpr iterator insert( const_iterator position, InIt first, InIt last )
    {
    }

    template <typename Range>
    constexpr iterator insert_range( const_iterator position, Range&& rng )
    {
    }

    constexpr iterator insert( const_iterator position, std::initializer_list<T> il )
    {
    }

    constexpr iterator erase( const_iterator position )
    {
    }

    constexpr iterator erase( const_iterator first, const_iterator last )
    {
    }

    constexpr void clear() noexcept
    {
    }

    constexpr void swap( inplace_vector& rhs )
      noexcept( Capacity == 0 || ( std::is_nothrow_swappable_v<T> &&
                                   std::is_nothrow_move_constructible_v<T> ) )
    {
    }

  private:

    // Synthesize a comparison operation even for objects that don't support <=> operator.
    // Used in operator<=> below
    struct SynthThreeWay
    {
      template <typename U, typename V>
      constexpr auto operator()( const U& lhs, const V& rhs ) const noexcept
        requires requires
      {
        { lhs < rhs } -> BooleanTestable;
        { lhs > rhs } -> BooleanTestable;
      }
      {
        if constexpr (std::three_way_comparable_with<U, V>)
          return lhs <=> rhs; // U supports operator <=>
        else
        { // implement <=> equivalent using existing < and > operators
          if (lhs < rhs)
            return std::strong_ordering::less;
          if (lhs > rhs)
            return std::strong_ordering::greater;
          return std::strong_ordering::equal;
        }
      }
    };

  public:

    friend constexpr bool operator==( const inplace_vector& lhs, const inplace_vector& rhs ) noexcept
    {
    }

    friend constexpr auto operator<=>( const inplace_vector& lhs, const inplace_vector& rhs ) noexcept
    {
      // Can't use std::array::operator<=> because must only compare a subset of elements
    }

    friend constexpr void swap( inplace_vector& lhs, inplace_vector& rhs )
      noexcept( Capacity == 0 || ( std::is_nothrow_swappable_v<T> &&
                                   std::is_nothrow_move_constructible_v<T> ) )
    {
      lhs.swap( rhs );
    }

  private:

    /*
    void CheckForEmptyStack() const
    {
#if defined(PK_ENABLE_EXCEPTIONS)
      if (empty())
        throw std::out_of_range( "empty stack" );
#else
      assert( !empty() );
#endif
    }

    void CheckForFullStack( [[maybe_unused]] size_t elementsToAdd ) const
    {
#if defined(PK_ENABLE_EXCEPTIONS)
      if (( size() + elementsToAdd ) > capacity())
        throw std::out_of_range( "stack overflow" );
#else
      assert( ( size() + elementsToAdd ) <= capacity() );
#endif
    }

  private:

    // top_ points to where the *next* element will be pushed
    // push(x) -> c_[top_] = x; ++top_;
    // pop()   -> --top_;
    // top()   -> return c_[top_-1];
    // empty() -> return top_ == 0;
    // end()   -> return c_ + top_;

    size_t top_ = 0;
    Array c_;
    */

  }; // class inplace_vector
  
} // namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////
