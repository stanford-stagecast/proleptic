#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"

using namespace std;

int main()
{
  auto devices = ALSADevices::list();

  for ( const auto& dev : devices ) {
    cout << "Found device: " << dev.name << "\n";

    for ( const auto& interface : dev.interfaces ) {
      cout << "     " << interface.first << " | " << interface.second << "\n";
    }
  }

  auto [name, interface_name] = ALSADevices::find_device( { "Scarlett" } );

  const auto device_claim = AudioDeviceClaim::try_claim( name );

  return 0;
}
