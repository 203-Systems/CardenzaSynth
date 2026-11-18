#pragma once

// The 203 community while the synth runs: CardenzaHAL's presence service (CardenzaHAL::Init starts it,
// net/presence.h), told what the synth is doing. Device layer only (ESP-IDF); Device::Community is its
// public face (Devices/Device.h).
//
// - RAM: the synth keeps kRamFloorBytes of internal RAM for itself (its effects and engines check for
//   16-20 KB free). Wi-Fi comes up only beside it and is let go below it: the service measures what Wi-Fi
//   takes and gives way.
// - Sound: while the speaker has had sound within kSoundHoldMs the activity is RealTime: Wi-Fi does not
//   come up in the middle of it (its first start in a run calibrates the radio on core 0), a connection
//   that is up keeps asking (a few ms of the network tasks every 15 s). Quiet (Wi-Fi let go) while the
//   microphone has the speaker's pin (a recording), while a sample plays from the card (its blocks need
//   internal RAM above 40 KB), and from the moment the voices wanted internal RAM Wi-Fi held (AMY put an
//   oscillator in PSRAM) until the sound stops. Otherwise Idle.
// - Handover (firmware sent from the website, or the Launcher's update): the service's task asks the
//   system loop to shut the app down as at an orderly power-off (its working state to the card, the
//   settings and the log written), waits for it (kHandoverWaitMs at most) and restarts into the Launcher.
namespace device::presence {

// Device::Init, after CardenzaHAL::Init: the floor, the handover hook, a log line on whether the
// service goes online in this run.
void Begin();
// The audio render task, for a block with sound in it (a 32-bit store).
void NoteSound();
// The microphone has the speaker's pin (DeviceAudio: PauseOutputForInput / ResumeOutputAfterInput).
void SetRecording(bool recording);
// The system loop, every frame: the activity follows the sound (a call into the service only on change).
void Frame();
// The system loop: a handover waits for the app to be shut down (os::system::Shutdown), then Prepared.
bool HandoverRequested();
void HandoverPrepared();

}  // namespace device::presence
