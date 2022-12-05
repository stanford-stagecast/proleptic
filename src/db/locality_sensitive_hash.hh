#pragma once

#include <Eigen/Dense>
#include <array>

#include "minhash.hh"
#include "random.hh"

template<std::integral T, size_t signature_length, size_t n_signatures, size_t prime, size_t n_buckets>
class LocalitySensitiveHash
{
  using SingleHash = MultiHash<T, 1, prime, n_buckets>;
  static_assert( prime > n_buckets );
  std::vector<SingleHash> hashers_ {};

public:
  LocalitySensitiveHash( std::default_random_engine& prng )
  {
    for ( size_t i = 0; i < n_signatures; i++ ) {
      hashers_.emplace_back( prng );
    }
  }

  Eigen::Vector<T, n_signatures> get_buckets( Eigen::Matrix<T, signature_length, n_signatures> signatures )
  {
    Eigen::Vector<T, n_signatures> buckets;
    for ( size_t i = 0; i < n_signatures; i++ ) {
      Eigen::Vector<T, signature_length> sig = signatures.col( i );
      Eigen::Vector<T, 1> hashes = hashers_[i].hash( sig.sum() );
      buckets[i] = hashes[0];
    }
    return buckets;
  }
};
