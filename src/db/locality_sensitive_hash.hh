#pragma once

#include <Eigen/Dense>
#include <array>

#include "minhash.hh"
#include "random.hh"

/// Given a vector of numbers of a certain size, produces a set of "buckets" to which to assign the vector for quick
/// similarity lookups. Similar vectors will ideally have overlapping sets of buckets.  The false positive vs. false
/// negative behavior of this algorithm can be controlled by adjusting the `signature_length` and `n_signatures`
/// parameters.
///
/// This algorithm is meant to be run on fixed-length "signatures" of real input data, where similar
/// data correspond to similar signatures.  For example, MinHash is a signature algorithm which is known to work
/// well with locality-sensitive hashing.  The large signature is divided into many small "bands", each of which is
/// then hashed to get a "bucket" number.  Two signatures will be placed in the same bucket if any band within them
/// is identical.
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

  Eigen::Vector<T, n_signatures> get_buckets( Eigen::Vector<T, signature_length * n_signatures> signatures )
  {
    Eigen::Matrix<T, signature_length, n_signatures> bands;
    for ( size_t i = 0; i < signature_length * n_signatures; i++ ) {
      bands( i / n_signatures, i % n_signatures ) = signatures[i];
    }
    return get_buckets( bands );
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
