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
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <new>
#include <ranges>
#include <stdexcept>

#pragma warning(push)
#pragma warning(disable: 26495) // "data_ is uninitialized", by design

namespace // anonymous
{
  // Requirements for object comparisons; see SynthThreeWay below
  template <typename T>
  concept BooleanTestableImpl = std::convertible_to<T, bool>;

  template <typename T>
  concept BooleanTestable = BooleanTestableImpl<T>
    && requires( T && a )
  {
    { !static_cast<T&&>( a ) } -> BooleanTestableImpl;
  };

  // Helper for standalone erase() and erase_if()
  template < typename DifferenceType >
  size_t asSizeType( DifferenceType d )
  {
    assert( d >= 0 && "difference can't be negative" );
    if constexpr ( sizeof( DifferenceType ) > sizeof( size_t ) )
    {
      assert( static_cast<std::make_unsigned_t< DifferenceType >>( d ) <=
              std::numeric_limits<size_t>::max() &&
              "difference exceeds size_t range" );
    }
    return static_cast<size_t>( d );
  }

}; // namespace anonymous

namespace PKIsensee
{

// Contiguous vector of objects T on stack; maximum size Capacity.
// Performs no memory allocations.

template < typename T, size_t Capacity > 
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

  // Constructors -------------------------------------------------------------

  constexpr inplace_vector() noexcept = default; // size() == 0

  constexpr explicit inplace_vector( size_type count )
    requires( std::constructible_from< T, T&& > && std::default_initializable<T> )
  {
    // default initialize first count elements
    assert( count <= capacity() );
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
    other.size_ = 0; // put moved-from object in valid but empty state
  }

  constexpr inplace_vector( std::initializer_list<T> iList )
    requires( std::constructible_from< T, std::ranges::range_reference_t< std::initializer_list< T > > > && 
              std::movable<T> )
  {
    insert( begin(), iList );
  }

  // Destructor ---------------------------------------------------------------

  constexpr ~inplace_vector()
  {
    destroy( begin(), end() );
  }

  // Assigment ----------------------------------------------------------------

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
    rhs.size_ = 0; // put moved-from object in valid but empty state
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

  // Element access -----------------------------------------------------------

  constexpr const_reference operator[]( size_type i ) const
  {
    assert( i < size_ );
    return ref( i );
  }

  constexpr reference operator[]( size_type i )
  {
    assert( i < size() );
    return ref( i );
  }

  constexpr const_reference at( size_type i ) const
  {
    if ( i >= size() )
      throw std::out_of_range( "inplace_vector::at" );
    return ref( i );
  }

  constexpr reference at( size_type i )
  {
    if ( i >= size() )
      throw std::out_of_range( "inplace_vector::at" );
    return ref( i );
  }

  constexpr const_reference front() const
  {
    assert( size() > 0 );
    return ref();
  }

  constexpr reference front()
  {
    assert( size() > 0 );
    return ref();
  }

  constexpr const_reference back() const
  {
    assert( size() > 0 );
    return ref( size() - 1 );
  }

  constexpr reference back()
  {
    assert( size() > 0 );
    return ref( size() - 1 );
  }

  constexpr const T* data() const noexcept
  {
    // For efficiency, does not return nullptr if empty() is true.
    // This is compliant behavior. To detect potential errors, debug versions
    // assert if the container is empty.
    assert( size() > 0 );
    return ptr();
  }

  constexpr T* data() noexcept
  {
    // For efficiency, does not return nullptr if empty() is true.
    // This is compliant behavior. To detect potential errors, debug versions
    // assert if the container is empty.
    assert( size() > 0 );
    return ptr();
  }

  // Iterators ----------------------------------------------------------------

  constexpr iterator begin() noexcept
  {
    return ptr();
  }

  constexpr const_iterator begin() const noexcept
  {
    return ptr();
  }

  constexpr const_iterator cbegin() const noexcept
  {
    return ptr();
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

  // Size and capacity --------------------------------------------------------

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

  // Resizing -----------------------------------------------------------------

public:

  constexpr void resize( size_type count )
    requires( std::constructible_from< T, const T& > && std::default_initializable<T> )
  {
    resizeImpl( count, []() { return T{}; } );
  }

  constexpr void resize( size_type count, const T& value )
    requires( std::constructible_from< T, const T& > )
  {
    resizeImpl( count, [&value]() -> const T& { return value; } );
  }

  static constexpr void reserve( size_type newCapacity )
  {
    // inplace_vector already reserves Capacity elements, so reserve() 
    // is a technically unnecessary but still required method
    if ( newCapacity > Capacity )
      throw std::bad_alloc();
  }

  static constexpr void shrink_to_fit() noexcept
  {
    // nothing required; no effects
  }

  // Modifiers ----------------------------------------------------------------

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
    // Function inserts new elements before pos
    assert( pos >= begin() && pos <= end() );
    if ( ( size() + count ) > capacity() )
      throw std::bad_alloc();

    // Add elements to the end and then rotate them into place
    auto newElementsPos = end();
    for ( size_type i = 0; i < count; ++i )
      unchecked_emplace_back( value );
    return rotate( pos, newElementsPos, end() );
  }

  template <class InIt>
  constexpr iterator insert( const_iterator pos, InIt first, InIt last )
    requires( std::constructible_from< T, std::iter_reference_t< InIt> >&& std::movable<T> )
  {
    assert( pos >= begin() && pos <= end() );
    assert( first <= last );
    auto count = last - first;
    if ( ( size() + count ) > capacity() )
      throw std::bad_alloc();

    // Add elements to the end and then rotate them into place
    auto newElementsPos = end();
    for ( ; first != last; ++first )
      unchecked_emplace_back( ::std::move( *first ) );
    return rotate( pos, newElementsPos, end() );
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
    // Function inserts new elements before pos
    assert( pos >= begin() && pos <= end() );

    // Add element to the end and then rotate it into place
    auto newElementPos = end();
    emplace_back( std::forward<Types>( values )... );
    return rotate( pos, newElementPos, end() );
  }

  template <typename... Types>
  constexpr reference emplace_back( Types&&... values )
    requires( std::constructible_from< T, Types... > )
  {
    auto newItem = try_emplace_back( std::forward<Types>( values )... );
    if ( newItem == nullptr )
      throw std::bad_alloc();
    return back();
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
    if ( ( size() + std::ranges::size(rng) ) > capacity() )
      throw std::bad_alloc();
    for ( auto&& e : rng )
      unchecked_emplace_back( std::forward< decltype( e ) >( e ) );
  }

  template <typename Range>
  constexpr auto try_append_range( Range&& rng ) -> std::ranges::borrowed_iterator_t<Range>
    requires( std::constructible_from< T, std::ranges::range_reference_t<Range>> )
  {
    // Returns an iterator pointing to the first element of rng that was not inserted,
    // or rng.end() if all elements were inserted. Does not provide the strong
    // exception guarantee; elements that were already inserted prior to a thrown
    // exception remain inserted.
    for( auto it = std::begin( rng ); it != std::end( rng ); ++it )
    {
      if ( size() == capacity() )
        return it;
      unchecked_emplace_back( std::forward<decltype( *it )>( *it ) );
    }
    return std::ranges::end( rng );
  }

  // Remove elements ----------------------------------------------------------

  constexpr void clear() noexcept
  {
    destroy( begin(), end() );
    size_ = 0;
  }

  constexpr iterator erase( const_iterator pos )
  {
    return erase( pos, pos + 1 );
  }

  constexpr iterator erase( const_iterator firstIt, const_iterator lastIt )
    requires( std::movable<T> )
  {
    // Make iterators consistent (all non-const) for return value and std::move algorithm.
    auto first = const_cast<iterator>( firstIt );
    auto last = const_cast<iterator>( lastIt );

    assert( first <= last );
    assert( first >= begin() && last <= end() );
    if ( first == last )
      return first;

    // move [last, end()) to first
    auto newLast = std::move( last, end(), first );
    destroy( newLast, end() );

    auto count = static_cast<size_type>( last - first );
    size_ -= count;
    return first; // iterator following last removed element
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

  template < typename ValueFactory >
  constexpr void resizeImpl( size_type count, ValueFactory&& getValue )
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
      return;
    }

    // Grow vector: append elements from factory
    while ( size() != count )
      emplace_back( getValue() );
  }

  template <class InIt>
  void destroy( InIt first, InIt last ) noexcept( std::is_nothrow_destructible_v<T> )
  {
    assert( first <= last );
    assert( first >= begin() && last <= end() );
    if constexpr ( !std::is_trivial_v<T> )
    {
      // dtors only necessary for non-trivial objects
      std::for_each( first, last, []( const auto& elem )
        {
          std::destroy_at( std::addressof( elem ) );
        } );
    }
  }

  // Synthesize a comparison operation even for objects that don't support <=> operator.
  // Used in operator<=> below
  struct synthThreeWay
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

  constexpr pointer ptr( size_t i = 0 ) noexcept
  {
    assert( i < Capacity );
    return reinterpret_cast<pointer>( data_ ) + i; // safe on aligned std::byte array of T
  }

  constexpr const_pointer ptr( size_t i = 0 ) const noexcept
  {
    assert( i < Capacity );
    return reinterpret_cast<const_pointer>( data_ ) + i; // safe on aligned std::byte array of T
  }

  constexpr reference ref( size_t i = 0 ) noexcept
  {
    return *ptr( i );
  }

  constexpr const_reference ref( size_t i = 0 ) const noexcept
  {
    return *ptr( i );
  }

  iterator rotate( const_iterator cfirst, iterator middle, iterator last )
  {
    // Iterators must be consistent (all non-const) in std::rotate
    auto first = const_cast<iterator>( cfirst );
    std::rotate( first, middle, last );
    return first;
  }

public:

  // Non-member functions -----------------------------------------------------

  friend constexpr bool operator==( const inplace_vector& lhs, const inplace_vector& rhs ) noexcept
  {
    if ( lhs.size() != rhs.size() )
      return false;
    return std::ranges::equal( lhs, rhs );
  }

  friend constexpr auto operator<=>( const inplace_vector& lhs, const inplace_vector& rhs ) noexcept
  {
    if ( lhs.size() < rhs.size() )
      return std::strong_ordering::less;
    if ( lhs.size() > rhs.size() )
      return std::strong_ordering::greater;

    // Sizes equivalent
    return std::lexicographical_compare_three_way( std::begin( lhs ), std::end( lhs ),
                                                   std::begin( rhs ), std::end( rhs ),
                                                   synthThreeWay{} );
  }

  friend constexpr void swap( inplace_vector& lhs, inplace_vector& rhs )
    noexcept( Capacity == 0 || ( std::is_nothrow_swappable_v<T> &&
                                 std::is_nothrow_move_constructible_v<T> ) )
  {
    lhs.swap( rhs );
  }

private:

  // Aligned bytes with sufficient size to store Capacity elements T on the stack.
  // Not default constructed for efficiency; just raw uninitialized memory.
  // construct_at() guarantees proper initialization.
  // 
  // For simplicity, this implementation doesn't specialize to have 
  // zero storage when Capacity is zero. Somes compilers will give a warning/error
  // if Capacity is zero.

  alignas( T ) std::byte data_[ sizeof( T ) * Capacity ];
  size_t size_ = 0;

  // size_ indicates where the *next* element will be push/emplace_back()'ed:
  // end()           -> begin() + size_;
  // emplace_back(x) -> construct_at( end(), x ); ++size_;
  // empty()         -> size_ == 0;
  //
  // For simplicity, this implementation doesn't use smaller types than size_t
  // for size_ even when Capacity is small (e.g. if Capacity was 100, uint8_t 
  // would be sufficient).

}; // class inplace_vector

// Non-member functions

template < typename T, size_t Capacity, class U = T >
constexpr auto erase( inplace_vector<T, Capacity>& vec, const U& value ) -> 
  inplace_vector<T, Capacity>::size_type
{
  // Erases all elements that compare equal to value
  auto it = std::remove( vec.begin(), vec.end(), value );
  auto countRemoved = std::distance( it, vec.end() );
  vec.erase( it, vec.end() );
  return asSizeType( countRemoved );
}

template < typename T, size_t Capacity, class Pred >
constexpr auto erase_if( inplace_vector<T, Capacity>& vec, Pred pred ) ->
  inplace_vector<T, Capacity>::size_type
{
  // Erases all elements that satisfy pred
  auto it = std::remove_if( vec.begin(), vec.end(), pred );
  auto countRemoved = std::distance( it, vec.end() );
  vec.erase( it, vec.end() );
  return asSizeType( countRemoved );
}

} // namespace PKIsensee

#pragma warning(pop)

///////////////////////////////////////////////////////////////////////////////
