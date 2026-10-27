#pragma once

namespace device {

// Call from the runtime/UI task. Audio workers only update lock-free counters.
void PollAudioDiagnostics();

}  // namespace device
