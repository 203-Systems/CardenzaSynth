// Compile the actual device control code in this TU so the test can initialize
// worker state without starting RTOS tasks. Native HAL modules are linked normally.
#include "Devices/Cardenza/DeviceAudio.cpp"
#include "esp_stub.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::size_t SilentRender(std::int16_t*, std::size_t frames) { return frames; }

void RequireSpeaker()
{
  Require(Fake::clockOwner == 1 && CardenzaHAL::Audio::GetTxChannel()->enabled &&
          CardenzaHAL::Audio::IsCodecReady() && !g_microphoneOwnsClock && !g_nativeInputRunning,
          "speaker clock/codec ownership was not restored");
}
void RequireMicrophone()
{
  Require(Fake::clockOwner == 2 && !CardenzaHAL::Audio::IsCodecReady() && g_microphoneOwnsClock &&
          g_nativeInputRunning && CardenzaHAL::Audio::GetTxChannel()->enabled && Fake::registers[0x14] == 0,
          "microphone did not exclusively own the pin with a muted codec and paced TX");
}
}

int main(int argc, char** argv)
{
  try
  {
    Require(argc == 2, "expected fault");
    Require(CardenzaHAL::Audio::Init() && CardenzaHAL::Audio::IsCodecReady(), "HAL codec init failed");
    (void)CardenzaHAL::Audio::SetVolume(38);
    Require(i2s_channel_enable(CardenzaHAL::Audio::GetTxChannel()) == ESP_OK, "TX enable failed");
    g_audioCounters.running.store(1);
    g_audioPumpStarted.store(true);
    g_audioRenderCallback.store(SilentRender);
    const std::string fault = argv[1];
    if (fault == "stop" || fault == "restore")
    {
      Require(Device::Audio::StartInput() == Device::Audio::InputStatus::Ready, "initial recording failed");
      RequireMicrophone();
      Fake::fail = fault == "stop" ? "mic_disable" : "speaker_pins";
      Device::Audio::StopInput();
      Require(g_microphoneOwnsClock && !CardenzaHAL::Audio::IsCodecReady(), "failed stop unmuted the speaker");
      if (fault == "stop") Require(Fake::clockOwner == 2, "failed RX stop stole its clock");
      Fake::fail.clear(); Fake::now += 1000001;
      device::PollAudioRecovery();
      RequireSpeaker();
    }
    else if (fault != "none")
    {
      Fake::fail = fault;
      Require(Device::Audio::StartInput() == Device::Audio::InputStatus::NotReady, "injected recording start failure was accepted");
      RequireSpeaker();
      Fake::fail.clear();
    }
    // Repeated switching catches reuse of an RX handle whose GPIO was restored to TX.
    for (int cycle = 0; cycle < 5; ++cycle)
    {
      Require(Device::Audio::StartInput() == Device::Audio::InputStatus::Ready, "recording could not restart");
      RequireMicrophone();
      Device::Audio::SetOutputVolume(61);
      Require(Fake::registers[0x14] == 0, "volume adjustment woke the speaker during recording");
      Device::Audio::StopInput();
      RequireSpeaker();
      Require(Fake::registers[0x14] == (61 * 191 + 50) / 100, "resume lost requested volume");
    }
    std::cout << "audio handoff/" << fault << " passed\n";
  }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
