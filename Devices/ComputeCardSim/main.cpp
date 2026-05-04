#include <iostream>
#include <memory>

#include "Applications/Application.h"
#include "Applications/Synth/SynthApp.h"
#include "Devices/Device.h"
#include "OS/Framework/Midi/MidiPacket.h"
#include "OS/MIDI/MIDI.h"

int main() {
  apps::RegisterSynthApplication();
  auto application = app::CreateApplication(app::MakeApplicationId("ComputeCard", "Synth"));
  if (!application)
  {
    std::cerr << "Failed to create Synth app\n";
    return 1;
  }

  auto* synthApp = dynamic_cast<apps::SynthApp*>(application.get());
  if (synthApp == nullptr)
  {
    std::cerr << "Synth app registry returned an unexpected type\n";
    return 1;
  }

  synthApp->Setup({});
  synthApp->PressMatrixKey(0, 0);
  synthApp->PressMatrixKey(0, 13);
  synthApp->ReleaseMatrixKey(0, 0);
  os::midi::Send(midi::MidiPacket::NoteOn(0, 60, 110), synthApp->Rack().PortForTrack(0));
  synthApp->Tick();
  synthApp->RenderFrame();
  Device::Display::Present();

  std::cout << Device::Name() << " [" << Device::BackendName() << "] "
            << Device::Display::kWidth << "x" << Device::Display::kHeight << "\n";
  std::cout << synthApp->DebugView();
  return 0;
}
