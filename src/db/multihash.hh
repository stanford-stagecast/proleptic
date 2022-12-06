#pragma once

#include <Eigen/Dense>

#include "random.hh"

template<std::integral T, size_t n_hashes, size_t prime, size_t m_buckets>
class MultiHash
{
  static_assert( prime > m_buckets );
  static_assert( m_buckets > n_hashes );

  using Vector = Eigen::Vector<T, n_hashes>;
  Vector coefficients_ {};
  Vector offsets_ {};

public:
  MultiHash( std::default_random_engine& prng )
  {
    auto distribution = std::uniform_int_distribution<unsigned>( 0, prime );
    for ( size_t i = 0; i < n_hashes; i++ ) {
      coefficients_[i] = distribution( prng );
      offsets_[i] = distribution( prng );
    }
  }

  Eigen::Vector<T, n_hashes> hash( T x ) const
  {
    Eigen::Vector<T, 1> v( x );
    return hash<1>( v );
  }

  template<size_t N>
  Eigen::Matrix<T, n_hashes, N> hash( const Eigen::Vector<T, N>& x ) const
  {
    using Matrix = Eigen::Matrix<T, n_hashes, N>;
    Matrix temp = ( coefficients_ * x.transpose() ).colwise() + offsets_;
    temp.noalias() = temp.unaryExpr( []( const T a ) { return static_cast<T>( a % static_cast<T>( prime ) ); } );
    temp.noalias()
      = temp.unaryExpr( []( const T a ) { return static_cast<T>( a % static_cast<T>( m_buckets ) ); } );
    return temp;
  }

  const Vector& coefficients() const { return coefficients_; }
  const Vector& offsets() const { return offsets_; }
};
