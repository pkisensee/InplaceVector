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
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <new>
#include <ranges>
#include <stdexcept>

// Uncomment the following line to enable exceptions, or define this symbol at build time
// #define PK_ENABLE_EXCEPTIONS 1

#if defined(PK_ENABLE_EXCEPTIONS)
#include <stdexcept>
constexpr auto PK_MAY_THROW = noexcept( false ); // indicates function may throw
#else
constexpr auto PK_MAY_THROW = noexcept( true ); // function will not throw
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
  template <typename T, size_t Capacity>
  class InplaceVecStorage
  {
  public:
    constexpr T* data( size_t i = 0 ) noexcept
    {
      assert( i < Capacity );
      return reinterpret_cast<T*>( blk_ ) + i;
    }

    constexpr const T* data( size_t i = 0 ) const noexcept
    {
      assert( i < Capacity );
      return reinterpret_cast<T*>( blk_ ) + i;
    }

    constexpr T& ref( size_t i = 0 ) noexcept
    {
      return *data( i );
    }

    constexpr const T& ref( size_t i = 0 ) const noexcept
    {
      return *data( i );
    }

  private:

    // Properly aligned bytes with sufficient size to store all elements T on the stack.
    // Not default constructed for max speed; just a raw uninitialized array of bytes.
    alignas( T ) std::byte blk_[ sizeof( T ) * Capacity ];
  };

  // Contiguous vector of objects T on stack; maximum size Capacity
  template <typename T, size_t Capacity> 
  class inplace_vector
  {
  public:

    using value_type             = T;
    using pointer                = T*;
    using const_pointer          = const T*;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // Constructors -----------------------------------------------------------

    constexpr inplace_vector() noexcept = default;

    constexpr explicit inplace_vector( size_type n )
      requires( std::constructible_from< T, T&& > && std::default_initializable<T> )
    {
      // default initialize first n elements
      assert( n < capacity() );
      for ( size_type i = 0; i < n; ++i )
        emplace_back( T{} );
    }

    constexpr inplace_vector( size_type n, const T& value )
      requires( std::constructible_from< T, const T& > && std::copyable<T> )
    {
      // initialize first n elements to contain value
      insert( begin(), n, value );
    }

    template <typename InIt>
    constexpr inplace_vector( InIt first, InIt last )
      requires( std::constructible_from< T, std::iter_reference_t<InIt>> && std::movable<T> )
    {
      insert( begin(), first, last );
    }

    template <typename Range>
    constexpr inplace_vector( std::from_range_t, Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> && std::movable<T> )
    {
      insert_range( begin(), std::forward<Range&&>( rng ) );
    }

    constexpr inplace_vector( const inplace_vector& c ) // copy ctor
      requires( std::copyable<T> )
    {
      for ( auto&& e : c )
        emplace_back( e );
    }

    constexpr inplace_vector( inplace_vector&& c ) // move ctor
      noexcept( Capacity == 0 || std::is_nothrow_move_constructible_v<T> )
      requires( std::movable<T> )
    {
      for ( auto&& e : c )
        emplace_back( std::move( e ) );
    }

    constexpr inplace_vector( std::initializer_list<T> iList )
      requires( std::constructible_from< T, std::ranges::range_reference_t<std::initializer_list<T>>> && 
                std::movable<T> )
    {
      insert( begin(), iList );
    }

    // Destructor -------------------------------------------------------------

    constexpr ~inplace_vector()
    {
      if constexpr( !std::is_trivial_v<T> )
      {
        for ( auto& e : *this )
          std::destroy_at( std::addressof( e ) );
      }
    }

    // Operators --------------------------------------------------------------

    constexpr inplace_vector& operator=( const inplace_vector& rhs ) // copy assignment
      requires( std::copyable<T> )
    {
      clear();
      for ( auto&& e : rhs )
        emplace_back( e );
      return *this;
    }

    constexpr inplace_vector& operator=( inplace_vector&& rhs ) // move assignment
      noexcept( Capacity == 0 || ( std::is_nothrow_move_assignable_v<T> &&
                                   std::is_nothrow_move_constructible_v<T> ) )
      requires( std::movable<T> )
    {
      clear();
      for ( auto&& e : rhs )
        emplace_back( std::move( e ) );
      return *this;
    }

    constexpr inplace_vector& operator=( std::initializer_list<T> iList )
      requires( std::constructible_from< T, std::ranges::range_reference_t<std::initializer_list<T>>> &&
                std::movable<T> )
    {
      assign( iList );
      return *this;
    }

    // Assignment -------------------------------------------------------------

    template <typename InIt>
    constexpr void assign( InIt first, InIt last )
      requires( std::constructible_from< T, std::iter_reference_t<InIt>> && std::movable<T> )
    {
      clear();
      insert( begin(), first, last );
    }

    template <typename Range>
    constexpr void assign_range( Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> && 
                std::movable<T> )
    {
      assign( std::begin( rng ), std::end( rng ) );
    }

    constexpr void assign( size_type n, const T& value )
      requires( std::constructible_from< T, const T& > && std::movable<T> )
    {
      clear();
      insert( begin(), n, value );
    }

    constexpr void assign( std::initializer_list<T> iList )
      requires( std::constructible_from< T, std::ranges::range_reference_t< std::initializer_list<T>>> && 
                std::movable<T> )
    {
      clear();
      insert_range( begin(), iList );
    }

    // Iterators --------------------------------------------------------------

    constexpr iterator begin() noexcept
    {
      return data_.data();
    }

    constexpr const_iterator begin() const noexcept
    {
      return data_.data();
    }

    constexpr const_iterator cbegin() const noexcept
    {
      return data_.data();
    }

    constexpr iterator end() noexcept
    {
      return begin() + size();
    }

    constexpr const_iterator end() const noexcept
    {
      return begin() + size();
    }

    constexpr const_iterator cend() const noexcept
    {
      return cbegin() + size();
    }

    constexpr reverse_iterator rbegin() noexcept
    {
      return reverse_iterator( end() );
    }

    constexpr const_reverse_iterator rbegin() const noexcept
    {
      return const_reverse_iterator( end() );
    }

    constexpr const_reverse_iterator crbegin() const noexcept
    {
      return const_reverse_iterator( cend() );
    }

    constexpr reverse_iterator rend() noexcept
    {
      return reverse_iterator( begin() );
    }

    constexpr const_reverse_iterator rend() const noexcept
    {
      return const_reverse_iterator( begin() );
    }

    constexpr const_reverse_iterator crend() const noexcept
    {
      return const_reverse_iterator( cbegin() );
    }

    // State ------------------------------------------------------------------

    constexpr bool empty() const noexcept
    {
      return ( size_ == 0 );
    }

    constexpr size_type size() const noexcept
    {
      return size_;
    }

    static constexpr size_type max_size() noexcept
    {
      return Capacity;
    }

    static constexpr size_type capacity() noexcept
    {
      return Capacity;
    }

    // Resizing ---------------------------------------------------------------

    constexpr void resize( size_type sz )
      requires( std::constructible_from< T, T&& > && std::default_initializable<T> )
    {
      if ( sz == size() )
        return;

      if ( sz > capacity() )
        throw std::bad_alloc();

      // Shrinking vector: erase last elements
      if ( sz < size() )
      {
        destroy( begin() + sz, end() );
        size_ = sz;
      }

      // Growing vector: append default-inserted elements
      else
      {
        while (size() != sz)
          emplace_back( T{} );
      }
    }

    constexpr void resize( size_type sz, const T& value )
    {
      if ( sz == size() )
        return;

      if ( sz > Capacity )
        throw std::bad_alloc();

      // Shrinking vector: erase last elements
      if ( sz < size() )
      {
        destroy( begin() + sz, end() );
        size_ = sz;
      }

      // Growing vector: append default-inserted elements
      else
        insert( end(), sz - size(), value );
    }

    static constexpr void reserve( size_type n )
    {
      // inplace_vector already reserves Capacity elements
      if ( n > Capacity )
        throw std::bad_alloc();
    }

    static constexpr void shrink_to_fit() noexcept
    {
      // nothing required; no effects
    }

    // Direct access ----------------------------------------------------------

    constexpr const_reference operator[]( size_type i ) const
    {
      assert( i < size_ );
      return data_.ref( i );
    }

    constexpr reference operator[]( size_type i )
    {
      assert( i < size() );
      return data_.ref( i );
    }

    constexpr const_reference at( size_type i ) const
    {
      if ( i >= size() )
        throw std::out_of_range( "inplace_vector::at" );
      return data_.ref( i );
    }

    constexpr reference at( size_type i )
    {
      if ( i >= size() )
        throw std::out_of_range( "inplace_vector::at" );
      return data_.ref( i );
    }

    constexpr const_reference front() const
    {
      assert( size() > 0 );
      return data_.ref();
    }

    constexpr reference front()
    {
      assert( size() > 0 );
      return data_.ref();
    }

    constexpr const_reference back() const
    {
      assert( size() > 0 );
      return data_.ref( size() - 1 );
    }

    constexpr reference back()
    {
      assert( size() > 0 );
      return data_.ref( size() - 1 );
    }

    constexpr const T* data() const noexcept
    {
      assert( size() > 0 );
      assert( data() == addressof( front() ) );
      return data_.data();
    }

    constexpr T* data() noexcept
    {
      assert( size() > 0 );
      assert( data() == addressof( front() ) );
      return data_.data();
    }

    // Add elements -----------------------------------------------------------

    constexpr reference push_back( const T& value )
      requires( std::constructible_from<T, const T&> )
    {
      emplace_back( value );
      return back();
    }

    constexpr reference push_back( T&& value )
      requires( std::constructible_from<T, T&&> )
    {
      emplace_back( std::forward<T&&>( value ) );
      return back();
    }

    template <typename Range>
    constexpr void append_range( Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> )
    {
      for ( auto&& e : rng )
      {
        if ( size() == capacity() )
          throw std::bad_alloc( "range too long in append_range" );
        emplace_back( std::forward<decltype( e )>( e ) );
      }
    }

    constexpr void pop_back()
    {
      assert( size() > 0 );
      destroy( end() - 1, end() );
      --size_;
    }

    template <typename... Types>
    constexpr pointer try_emplace_back( Types&&... values )
    {
      if ( size() == capacity() )
        return nullptr;

      assert( size() < capacity() );
      std::construct_at( end(), std::forward<Types>( values )... );
      ++size_;
      return std::addressof( back() );
    }

    constexpr pointer try_push_back( const T& value )
      requires( std::constructible_from<T, const T&> )
    {
      return try_emplace_back( value );
    }

    constexpr pointer try_push_back( T&& value )
      requires( std::constructible_from<T, T&&> )
    {
      return try_emplace_back( std::forward<T&&>( value ) );
    }

    template <typename Range>
    constexpr std::ranges::borrowed_iterator_t<Range> try_append_range( Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> )
    {
      // Returns an iterator pointing to the first element of rng that was not inserted,
      // or ranges::end(rng) if all elements were inserted
      for( auto it = std::begin(rng); it != std::end(rng); ++it )
      {
        if ( size() == capacity() )
          return it;
        emplace_back( std::forward<decltype( *it )>( *it ) );
      }
      return std::ranges::end( rng );
    }

    template <typename... Types>
    constexpr reference unchecked_emplace_back( Types&&... values )
      requires( std::constructible_from< T, Types... > )
    {
      return *try_emplace_back( std::forward<Types>( values )... );
    }

    constexpr reference unchecked_push_back( const T& value )
    {
      return *try_push_back( std::forward<decltype(value)>( value ) );
    }

    constexpr reference unchecked_push_back( T&& value )
    {
      return *try_push_back( std::forward<decltype( value )>( value ) );
    }

    template <typename... Types>
    constexpr iterator emplace( const_iterator position, Types&&... values )
      requires( std::constructible_from< T, Types...> && std::movable<T> )
    {
      // inserts a new element directly before 'position' by adding it
      // to the end and then rotating it into place
      auto newElementPos = end();
      emplace_back( std::forward<Types>( values )... );
      std::rotate( position, newElementPos, end() );
      return position;
    }

    constexpr iterator insert( const_iterator position, const T& value )
      requires( std::constructible_from< T, const T& > && std::copyable<T> )
    {
      return insert( position, 1, value );
    }

    constexpr iterator insert( const_iterator position, T&& value )
      requires( std::constructible_from< T, T&& > && std::movable<T> )
    {
      return emplace( position, ::std::move( value ) );
    }

    constexpr iterator insert( const_iterator position, size_type n, const T& value )
      requires( std::constructible_from< T, const T& > && std::copyable<T> )
    {
      // inserts new elements directly before 'position' by adding them
      // to the end and then rotating them into place
      auto newElementsStartPos = end();
      for ( size_type i = 0; i < n; ++i )
        emplace_back( value );
      std::rotate( position, newElementsStartPos, end() );
      return position;
    }

    template <class InIt>
    constexpr iterator insert( const_iterator position, InIt first, InIt last )
      requires( std::constructible_from< T, std::iter_reference_t< InIt> > && std::movable<T> )
    {
      // inserts new elements directly before 'position' by adding them
      // to the end and then rotating them into place
      auto newElementsStartPos = end();
      for ( ; first != last; ++first )
        emplace_back( ::std::move( *first ) );
      std::rotate( position, newElementsStartPos, end() );
      return position;
    }

    template <typename Range>
    constexpr iterator insert_range( const_iterator position, Range&& rng )
    {
    }

    constexpr iterator insert( const_iterator position, std::initializer_list<T> iList )
    {
    }

    // Remove elements --------------------------------------------------------

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
        if constexpr ( std::three_way_comparable_with<U, V> )
          return lhs <=> rhs; // U supports operator <=>
        else
        { // implement <=> equivalent using existing < and > operators
          if ( lhs < rhs )
            return std::strong_ordering::less;
          if ( lhs > rhs )
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

    InplaceVecStorage< T, Capacity > data_;

    // size_ indicates where the *next* element will be push_back()'ed
    // end()        -> return data_.data() + size_;
    // push_back(x) -> construct_at(end(), x); ++size_;
    // empty()      -> return size_ == 0;
    size_t size_ = 0;

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
