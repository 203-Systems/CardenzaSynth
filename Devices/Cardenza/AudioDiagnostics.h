#pragma once

namespace device {

// Call from the runtime/UI task. Audio workers only update lock-free counters.
void PollAudioDiagnostics();
// UI/control context only. Healthy playback performs no I2C or recovery work.
void PollAudioRecovery();

}  // namespace device
