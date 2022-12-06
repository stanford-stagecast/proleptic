#pragma once

#include "multihash.hh"

/// Given a set, calculate a MinHash signature of it.  This signature (a vector of integers) has the property that
/// the expected value of the number of entries which are identical in two signatures is equal to the Jaccard
/// similarity of the two sets used to compute those signatures.
///
/// Since this class randomly generates hash functions, there should be one global instance of it from which all
/// signatures are computed. Signatures computed using different instances of this class will be incompatible.
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
