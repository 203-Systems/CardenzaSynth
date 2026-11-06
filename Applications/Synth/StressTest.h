#pragma once

namespace apps {

class GrooveApp;

// The on-board stress test (Applications/Synth/StressTest.cpp): `make DEVICE=Cardenza stress` builds a
// firmware with it (CARDENZA_STRESS_TEST, never a release) that, 20 s after boot, writes extreme
// projects and old project files to the card, loads and plays them, works the device while they play
// (engine switches, loads, saves, a storm of keys) and measures each scenario: audio underruns and
// overruns, voices faded and shed, late sequencer pulses, UI frame times, the least internal RAM and
// PSRAM. It survives a crash or a watchdog reset and goes on with the next scenario. Results go to
// the console and the log (`stress:` lines).
void StressTestTick(GrooveApp& app);

}  // namespace apps
