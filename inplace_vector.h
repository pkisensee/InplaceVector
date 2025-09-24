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
#include <vector>

namespace // anonymous
{
  // Requirements for object comparisons; see SynthThreeWay below
  template <typename T>
  concept BooleanTestableImpl = std::convertible_to<T, bool>;

  template <typename T>
  concept BooleanTestable = BooleanTestableImpl<T>
    && requires( T && a ) {
      { !static_cast<T&&>( a ) } -> BooleanTestableImpl;
  };

}; // namespace anonymous

namespace PKIsensee
{
  template <typename T, size_t Capacity>
  class InplaceVecStorage
  {
  public:
    constexpr T* getPtr( size_t i = 0 ) noexcept
    {
      assert( i < Capacity );
      return reinterpret_cast<T*>( blk_ ) + i;
    }

    constexpr const T* getPtr( size_t i = 0 ) const noexcept
    {
      assert( i < Capacity );
      return reinterpret_cast<T*>( blk_ ) + i;
    }

    constexpr T& getRef( size_t i = 0 ) noexcept
    {
      return *getPtr( i );
    }

    constexpr const T& getRef( size_t i = 0 ) const noexcept
    {
      return *getPtr( i );
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

    constexpr inplace_vector() noexcept = default; // data() == nullptr and size() == 0

    constexpr explicit inplace_vector( size_type count )
      requires( std::constructible_from< T, T&& > && std::default_initializable<T> )
    {
      // default initialize first count elements
      assert( count < capacity() );
      for ( size_type i = 0; i < count; ++i )
        emplace_back( T{} );
    }

    constexpr inplace_vector( size_type count, const T& value )
      requires( std::constructible_from< T, const T& > && std::copyable<T> )
    {
      // initialize first count elements to contain value
      insert( begin(), count, value );
    }

    template <typename InIt>
    constexpr inplace_vector( InIt first, InIt last )
      requires( std::constructible_from< T, std::iter_reference_t< InIt > > && std::movable<T> )
    {
      insert( begin(), first, last );
    }

    template <typename Range>
    constexpr inplace_vector( std::from_range_t, Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> && std::movable<T> )
    {
      insert_range( begin(), std::forward<Range&&>( rng ) );
    }

    constexpr inplace_vector( const inplace_vector& other ) // copy ctor
      requires( std::copyable<T> )
    {
      for ( auto&& e : other )
        emplace_back( e );
    }

    constexpr inplace_vector( inplace_vector&& other ) // move ctor
      noexcept( Capacity == 0 || std::is_nothrow_move_constructible_v<T> )
      requires( std::movable<T> )
    {
      for ( auto&& e : other )
        emplace_back( std::move( e ) );
    }

    constexpr inplace_vector( std::initializer_list<T> iList )
      requires( std::constructible_from< T, std::ranges::range_reference_t< std::initializer_list< T > > > && 
                std::movable<T> )
    {
      insert( begin(), iList );
    }

    // Destructor -------------------------------------------------------------

    constexpr ~inplace_vector()
    {
      destroy( begin(), end() );
    }

    // Assigment --------------------------------------------------------------

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
      requires( std::constructible_from< T, std::ranges::range_reference_t< std::initializer_list< T > > > &&
                std::movable<T> )
    {
      assign( iList );
      return *this;
    }

    constexpr void assign( size_type count, const T& value )
      requires( std::constructible_from< T, const T& > && std::movable<T> )
    {
      clear();
      insert( begin(), count, value );
    }

    template <typename InIt>
    constexpr void assign( InIt first, InIt last )
      requires( std::constructible_from< T, std::iter_reference_t< InIt > > && std::movable<T> )
    {
      clear();
      insert( begin(), first, last );
    }

    constexpr void assign( std::initializer_list<T> iList )
      requires( std::constructible_from< T, std::ranges::range_reference_t< std::initializer_list< T > > > &&
                std::movable<T> )
    {
      clear();
      insert_range( begin(), iList );
    }

    template <typename Range>
    constexpr void assign_range( Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t< Range > > && 
                std::movable<T> )
    {
      assign( std::begin( rng ), std::end( rng ) );
    }

    // Element access ---------------------------------------------------------

    constexpr const_reference operator[]( size_type i ) const
    {
      assert( i < size_ );
      return data_.getRef( i ); // TODO reference()
    }

    constexpr reference operator[]( size_type i )
    {
      assert( i < size() );
      return data_.getRef( i );
    }

    constexpr const_reference at( size_type i ) const
    {
      if ( i >= size() )
        throw std::out_of_range( "inplace_vector::at" );
      return data_.getRef( i );
    }

    constexpr reference at( size_type i )
    {
      if ( i >= size() )
        throw std::out_of_range( "inplace_vector::at" );
      return data_.getRef( i );
    }

    constexpr const_reference front() const
    {
      assert( size() > 0 );
      return data_.getRef();
    }

    constexpr reference front()
    {
      assert( size() > 0 );
      return data_.getRef();
    }

    constexpr const_reference back() const
    {
      assert( size() > 0 );
      return data_.getRef( size() - 1 );
    }

    constexpr reference back()
    {
      assert( size() > 0 );
      return data_.getRef( size() - 1 );
    }

    constexpr const T* data() const noexcept
    {
      // For efficiency, this implementation does not return nullptr if empty() is true.
      // This is standard compliant. In order to detect potential errors, debug versions
      // will assert if the container is empty.
      assert( size() > 0 );
      return data_.getPtr();
    }

    constexpr T* data() noexcept
    {
      // For efficiency, this implementation does not return nullptr if empty() is true.
      // This is standard compliant. In order to detect potential errors, debug versions
      // will assert if the container is empty.
      assert( size() > 0 );
      return data_.getPtr();
    }

    // Iterators --------------------------------------------------------------

    constexpr iterator begin() noexcept
    {
      return data_.getPtr();
    }

    constexpr const_iterator begin() const noexcept
    {
      return data_.getPtr();
    }

    constexpr const_iterator cbegin() const noexcept
    {
      return data_.getPtr();
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

    // Size and capacity ------------------------------------------------------

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

    constexpr void resize( size_type count )
      requires( std::constructible_from< T, const T& > && std::default_initializable<T> )
    {
      resize( count, T{} );
    }

    constexpr void resize( size_type count, const T& value )
      requires( std::constructible_from< T, const T& > )
    {
      if ( count == size() )
        return;

      if ( count > capacity() )
        throw std::bad_alloc();

      // Shrink vector: erase last elements
      if ( count < size() )
      {
        destroy( begin() + count, end() );
        size_ = count;
      }

      // Grow vector: append default-inserted elements
      else
      {
        while ( size() != count )
          emplace_back( T{} );
      }
    }

    static constexpr void reserve( size_type newCapacity )
    {
      // inplace_vector already reserves Capacity elements, so reserve() 
      // is technically unnecessary but still a required method
      if ( newCapacity > Capacity )
        throw std::bad_alloc();
    }

    static constexpr void shrink_to_fit() noexcept
    {
      // nothing required; no effects
    }

    // Modifiers --------------------------------------------------------------

    constexpr iterator insert( const_iterator pos, const T& value )
      requires( std::constructible_from< T, const T& > && std::copyable<T> )
    {
      return insert( pos, 1, value );
    }

    constexpr iterator insert( const_iterator pos, T&& value )
      requires( std::constructible_from< T, T&& >&& std::movable<T> )
    {
      return emplace( pos, ::std::move( value ) );
    }

    constexpr iterator insert( const_iterator pos, size_type count, const T& value )
      requires( std::constructible_from< T, const T& > && std::copyable<T> )
    {
      assert( pos >= begin() && pos <= end() );
      // Inserts new elements before pos by adding them to the end and 
      // then rotating them into place
      auto newElementsStartPos = end();
      for ( size_type i = 0; i < count; ++i )
        emplace_back( value );
      std::rotate( pos, newElementsStartPos, end() );
      return pos;
    }

    template <class InIt>
    constexpr iterator insert( const_iterator pos, InIt first, InIt last )
      requires( std::constructible_from< T, std::iter_reference_t< InIt> >&& std::movable<T> )
    {
      assert( pos >= begin() && pos <= end() );
      // Inserts new elements before pos by adding them to the end and 
      // then rotating them into place
      auto newElementsStartPos = end();
      for ( ; first != last; ++first )
        emplace_back( ::std::move( *first ) );
      std::rotate( pos, newElementsStartPos, end() );
      return pos;
    }

    constexpr iterator insert( const_iterator pos, std::initializer_list<T> iList )
      requires( std::constructible_from< T, std::ranges::range_reference_t< std::initializer_list< T > > > &&
                std::movable<T> )
    {
      return insert( pos, std::begin( iList ), std::end( iList ) );
    }

    template <typename Range>
    constexpr iterator insert_range( const_iterator pos, Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t< Range > > && 
                std::movable<T> )
    {
      return insert( pos, std::begin( rng ), std::end( rng ) );
    }

    template <typename... Types>
    constexpr iterator emplace( const_iterator pos, Types&&... values )
      requires( std::constructible_from< T, Types... > && std::movable<T> )
    {
      assert( pos >= begin() && pos <= end() );
      // Inserts a new element before pos by adding it to the end and 
      // then rotating it into place
      auto newElementPos = end();
      emplace_back( std::forward<Types>( values )... );
      std::rotate( pos, newElementPos, end() );
      return pos;
    }

    template <typename... Types>
    constexpr reference emplace_back( Types&&... values )
      requires( std::constructible_from< T, Types... > )
    {
      if ( !try_emplace_back( std::forward<Types>( values )... ) )
        throw std::bad_alloc();
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

    template <typename... Types>
    constexpr reference unchecked_emplace_back( Types&&... values )
      requires( std::constructible_from< T, Types... > )
    {
      assert( size() < capacity() );
      return *try_emplace_back( std::forward<Types>( values )... );
    }

    constexpr reference push_back( const T& value )
      requires( std::constructible_from< T, const T& > )
    {
      emplace_back( value );
      return back();
    }

    constexpr reference push_back( T&& value )
      requires( std::constructible_from< T, T&& > )
    {
      emplace_back( std::forward<T&&>( value ) );
      return back();
    }

    constexpr pointer try_push_back( const T& value )
      requires( std::constructible_from< T, const T& > )
    {
      return try_emplace_back( value );
    }

    constexpr pointer try_push_back( T&& value )
      requires( std::constructible_from< T, T&& > )
    {
      return try_emplace_back( std::forward<T&&>( value ) );
    }

    constexpr reference unchecked_push_back( const T& value )
    {
      assert( size() < capacity() );
      return *try_push_back( std::forward< decltype( value ) >( value ) );
    }

    constexpr reference unchecked_push_back( T&& value )
    {
      assert( size() < capacity() );
      return *try_push_back( std::forward< decltype( value ) >( value ) );
    }

    constexpr void pop_back()
    {
      assert( size() > 0 );
      destroy( end() - 1, end() );
      --size_;
    }

    template <typename Range>
    constexpr void append_range( Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t< Range > > )
    {
      for ( auto&& e : rng )
        emplace_back( std::forward< decltype( e ) >( e ) );
    }

    template <typename Range>
    constexpr std::ranges::borrowed_iterator_t<Range> try_append_range( Range&& rng )
      requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> )
    {
      // Returns an iterator pointing to the first element of rng that was not inserted,
      // or rng.end() if all elements were inserted
      for( auto it = std::begin(rng); it != std::end(rng); ++it )
      {
        if ( size() == capacity() )
          return it;
        emplace_back( std::forward<decltype( *it )>( *it ) );
      }
      return std::ranges::end( rng );
    }

    // Remove elements --------------------------------------------------------

    constexpr void clear() noexcept
    {
      destroy( begin(), end() );
      size_ = 0;
    }

    constexpr iterator erase( const_iterator pos )
    {
      erase( pos, pos + 1 );
    }

    constexpr iterator erase( const_iterator first, const_iterator last )
    {
      assert( first <= last );
      assert( first >= begin() && last <= end() );
      if ( first == last )
        return first;
      // move [ last, end() ) to first
      auto newLast = std::move( last, end(), first );
      destroy( newLast, end() );
      auto count = static_cast<size_type>( last - first );
      size_ -= count;
      return first; // the iterator following last removed element
    }

    constexpr void swap( inplace_vector& rhs )
      noexcept( Capacity == 0 || ( std::is_nothrow_swappable_v<T> &&
                                   std::is_nothrow_move_constructible_v<T> ) )
    {
      auto tmp = std::move( rhs );
      rhs      = std::move( *this );
      *this    = std::move( tmp );
    }

  private:

    template <class InIt>
    void destroy( InIt first, InIt last ) noexcept( std::is_nothrow_destructible_v<T> )
    {
      if constexpr ( !std::is_trivial_v<T> )
      {
        std::for_each( first, last, []( const auto& elem )
          {
            std::destroy_at( std::addressof( elem ) );
          } );
      }
    }

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
      return ( lhs.size() == rhs.size() ) && std::ranges::equal( lhs, rhs );
    }

    friend constexpr auto operator<=>( const inplace_vector& lhs, const inplace_vector& rhs ) noexcept
    {
      if ( lhs.size() < rhs.size() )
        return -1;
      if ( lhs.size() > rhs.size() )
        return 1;

      // Sizes are equivalent
      return std::lexicographical_compare_three_way( std::begin( lhs ), std::end( lhs ),
                                                     std::begin( rhs ), std::end( rhs ),
                                                     SynthThreeWay{} );
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
