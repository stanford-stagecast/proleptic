# Database Utilities

## MultiHash: `vector<int> -> matrix<int>`

This hashes a vector input using many different hash functions. It
returns the matrix of each hash function applied to each element of the
vector. This is order-sensitive (i.e., the input is a vector, not a set).

## MinHash: `set<int> -> vector<int>`

This is a specialization of MultiHash which returns whichever hash in
a row of the matrix has a minimum numerical value. This is useful because
the number of elements which are the same in the MinHashes of two
different sets is approximately equal to the Jaccard similarity of the two
sets, but the MinHash is much more efficient to calculate. This is
order-insensitive (i.e., the input is a set), but the output signature is
ordered (i.e., the output is a vector).

## Locality-Sensitive Hash: `vector<int> -> set<int>`

This algorithm assigns an input vector to a set of hash "buckets", such
two similar vectors will usually overlap in at least one bucket. This is
useful for collecting a set of similar candidates to an input, but the
true similarity should be checked with MinHash or Jaccard later to
eliminate false positives. This is order-sensitive (i.e., the input is
a vector), but the output is order-insensitive (i.e., the output is
a set).

Given an arbitrary input, one can build a DB index for it by:

- Converting that input into a meaningful set of numbers, where set
  similarity corresponds to input similarity. This is domain-specific; for
  text, it's common to create "shingles", which act like word embeddings.
  It may be possible to use an autoencoder to perform this step, but it's
  far more common to compute it traditionally.
- MinHashing the set of numbers to get a signature
- LSHing the signature to get a set of buckets
- Storing a reference to the original input data in every bucket indicated
  by the LSH

To later look up the most similar entries to a new input:

- Convert that input into a set of numbers.
- MinHash the set to a signature.
- LSH the signature to buckets.
- Take the union of all the candidates in every bucket indicated by the
  LSH.
- Compute the MinHash or Jaccard similarity between every candidate and
  the input. Filter out any candidates with low similarity.
