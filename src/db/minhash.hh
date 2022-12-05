#pragma once

#include "multihash.hh"

template<std::integral T, size_t n_hashes, size_t prime, size_t m_buckets>
class MinHash : public MultiHash<T, n_hashes, prime, m_buckets>
{
public:
  MinHash( std::default_random_engine& prng )
    : MultiHash<T, n_hashes, prime, m_buckets>( prng )
  {}

  T minhash( const T& x ) const
  {
    Eigen::Vector<T, 1> v( x );
    return minhash<1>( v )( 0 );
  }

  template<size_t N>
  Eigen::Vector<T, n_hashes> minhash( const Eigen::Vector<T, N>& x ) const
  {
    return this->template hash<N>( x ).rowwise().minCoeff();
  }
};
