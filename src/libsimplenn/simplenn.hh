#pragma once
#include <array>
#include <memory>
#include <string_view>

struct NetworkData;

class SimpleNN
{
public:
  //! Create a new instance of the network described by the specified file.
  //!
  //! \param[in] filename  A file containing a serialized neural network.
  //!
  //! \note  The network in the file has to match the network structure currently expected by libsimplenn, which
  //! might change over time.
  SimpleNN( const std::string& filename );

  //! Make a single prediction about the next note timestamp.
  //!
  //! \param[in] past_timestamps A list of previous note timestamps, starting from the most recent (the lowest
  //! value) and going up to the 16th most recent.  The values should be in seconds relative to the time when this
  //! function is called, where positive values are in the past and negative values are in the future.
  //!
  //! \return The predicted next timestamp, in seconds relative to when this function was called.  Positive values
  //! are in the past and negative values are in the future.
  float predict_next_timestamp( const std::array<float, 16>& past_timestamps );

private:
  std::unique_ptr<NetworkData> data_ {};
};
