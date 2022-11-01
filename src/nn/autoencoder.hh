#pragma once
#include "network.hh"
#include <memory>

template<std::floating_point T, size_t input_size, size_t hidden_size>
struct Autoencoder
{
  using Codec = Network<T, input_size, hidden_size, input_size>;
  using Encoder = Network<T, input_size, hidden_size>;
  using Decoder = Network<T, hidden_size, input_size>;

private:
  std::unique_ptr<Encoder> encoder_;
  std::unique_ptr<Decoder> decoder_;

public:
  Autoencoder( const Codec& nn )
    : encoder_( std::make_unique<Encoder>() )
    , decoder_( std::make_unique<Decoder>() )
  {
    Encoder& enc = *encoder_;
    enc.first.weights = nn.first.weights;
    enc.first.biases = nn.first.biases;

    Decoder& dec = *decoder_;
    dec.first.weights = nn.rest.first.weights;
    dec.first.biases = nn.rest.first.biases;
  }

  const Encoder& encoder() const { return *encoder_; }

  const Decoder& decoder() const { return *decoder_; }
};
