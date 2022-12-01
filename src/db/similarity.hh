#pragma once

#include <algorithm>
#include <set>
#include <vector>

#include <assert.h>

//! Calculates the Jaccard Index (the basis for MinHash) naively; this is slow and should probably only be used for
//! checking faster methods.
//! Fails if b.empty()
template<class T>
float jaccard_index( std::set<T> a, std::set<T> b )
{
  assert( !b.empty() );
  std::vector<T> intersection( a.size() + b.size() );
  auto it = std::set_intersection( a.begin(), a.end(), b.begin(), b.end(), intersection.begin() );
  intersection.resize( it - intersection.begin() );

  std::vector<T> union_( a.size() + b.size() );
  it = std::set_union( a.begin(), a.end(), b.begin(), b.end(), union_.begin() );
  union_.resize( it - union_.begin() );
  return intersection.size() / static_cast<float>( union_.size() );
}
