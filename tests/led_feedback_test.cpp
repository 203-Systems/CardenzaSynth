#include "Applications/Synth/UI/SynthLedFeedback.h"
#include "Devices/Cardenza/cardenza/keyboard/internal/led_pwm.h"
#include "Devices/LedFrameBuffer.h"

#include <array>
#include <cassert>
#include <iostream>

int main()
{
  using namespace CardenzaHAL::Keyboard::detail;
  // Exercise the production integer PWM math without mocking ESP-IDF or GPIO.
  std::array<bool, kLedPwmPhaseCount> ranks{};
  for (unsigned phase = 0; phase < kLedPwmPhaseCount; ++phase)
  {
    const auto rank = LedPhaseRank(phase);
    assert(rank < ranks.size() && !ranks[rank]);
    ranks[rank] = true;
  }
  unsigned lastDuty = 0;
  for (unsigned level = 0; level <= 15; ++level)
  {
    unsigned duty = 0;
    for (unsigned phase = 0; phase < kLedPwmPhaseCount; ++phase)
      duty += LedPhaseOn(level, 15, phase);
    assert(level == 0 ? duty == 0 : duty >= lastDuty);
    lastDuty = duty;
    for (unsigned master = 0; master <= 15; ++master)
    {
      assert(LedDuty(level, master) == LedDuty(master, level));
      assert(LedDuty(level, master) <= kLedPwmPhaseCount);
      if (master == 0 || level == 0) assert(LedDuty(level, master) == 0);
      else assert(LedDuty(level, master) > 0);
      if (master > 0) assert(LedDuty(level, master) >= LedDuty(level, master - 1));
    }
  }
  assert(lastDuty == kLedPwmPhaseCount);
  assert(LedDuty(255, 255) == kLedPwmPhaseCount);
  // Stable roles must occupy separate fast PWM stages at the default master.
  assert(LedDuty(synth::led::kAvailable, 15) == 1);
  assert(LedDuty(synth::led::kContent, 15) == 2);
  assert(LedDuty(synth::led::kSelected, 15) == 3);
  assert(LedDuty(synth::led::kPressed, 15) == 4);
  int lastNotification = -4;
  for (int cycle = 0; cycle < 48; ++cycle)
    if (NotifyKeysAtPhase(cycle % kLedPwmPhaseCount))
    {
      assert(cycle - lastNotification == 4);
      lastNotification = cycle;
    }

  synth::UiScreenSnapshot screen;
  synth::UiBarItemSnapshot item;
  assert(synth::led::BarLevel(item, screen) == 0);
  item.label = "VEL";
  const auto available = synth::led::BarLevel(item, screen);
  item.active = true;
  const auto selected = synth::led::BarLevel(item, screen);
  item.pressed = true;
  assert(synth::led::BarLevel(item, screen) > selected && selected > available);
  assert(available > synth::led::kAvailable);  // Function rails separate from the quiet grid.
  item.disabled = true;
  assert(synth::led::BarLevel(item, screen) == 0);
  item = {}; item.icon = synth::UiBarIcon::Play; item.playing = true;
  screen.sequencer.patternProgress = 0;
  const auto beat = synth::led::BarLevel(item, screen);
  screen.sequencer.patternProgress = .125f;
  assert(beat == synth::led::BarLevel(item, screen));
  item.playing = false; item.danger = true; screen.samplerRecording = true;
  assert(synth::led::BarLevel(item, screen) == synth::led::kPressed);

  synth::SequencerStepUiState step;
  assert(synth::led::StepLevel(step, true, false) > 0);
  step.hasEvents = true;
  const auto content = synth::led::StepLevel(step, true, false);
  step.held = true;
  const auto edit = synth::led::StepLevel(step, true, false);
  assert(edit > content);
  assert(synth::led::StepLevel(step, false, false) > edit);
  step.playing = true;
  assert(synth::led::StepLevel(step, true, false) > edit);
  step.held = step.playing = false;
  assert(synth::led::StepLevel(step, true, true) == synth::led::kSelected);

  screen = {};
  const auto pitch = synth::led::KeyboardLevel(screen, 3, true, false);
  const auto root = synth::led::KeyboardLevel(screen, 3, true, true);
  assert(root > pitch && pitch > 0);
  screen.showSequencerGrid = true;
  assert(synth::led::KeyboardLevel(screen, 3, true, false) == 0);
  assert(synth::led::KeyboardLevel(screen, 3, true, true) > 0);
  screen.sequencer.hasHeldSteps = true;
  assert(synth::led::KeyboardLevel(screen, 3, true, false) == pitch);
  screen.sequencer.editValueCount = 6;
  screen.sequencer.editValueIndex = 2;
  assert(synth::led::EditValueLevel(screen.sequencer, 0, 1) == 0);
  assert(synth::led::EditValueLevel(screen.sequencer, 3, 1) > 0);
  assert(synth::led::EditValueLevel(screen.sequencer, 0, 2) > synth::led::EditValueLevel(screen.sequencer, 3, 1));
  assert(synth::led::EditValueLevel(screen.sequencer, 3, 6) == 0);

  Device::LED::detail::LedFrameBuffer frames;
  assert(frames.SetEnabled(true));
  assert(frames.Fill(6));
  const auto previous = frames.Current();
  frames.BeginFrame();
  assert(!frames.Fill(0) && !frames.SetEnabled(false));
  assert(frames.Current().enabled && frames.Current().levels == previous.levels);
  assert(!frames.SetKey(0, 9, 15) && !frames.SetEnabled(true));
  assert(frames.Current().levels == previous.levels);
  assert(frames.Present());
  assert(frames.Current().enabled && frames.Current().levels[9] == 15 && frames.Current().levels[8] == 0);
  // Rebuilding the same display must never publish a blank intermediate frame.
  for (int frame = 0; frame < 100; ++frame)
  {
    frames.BeginFrame();
    assert(!frames.Fill(0));
    assert(frames.Current().enabled && frames.Current().levels[9] == 15);
    assert(!frames.SetKey(0, 9, 15));
    assert(!frames.Present());
  }
  // Explicit power-off outside a transaction remains immediate.
  assert(frames.SetEnabled(false) && !frames.Current().enabled);
  assert(frames.SetEnabled(true) && frames.Current().levels[9] == 15);
  assert(!frames.SetKey(4, 14, 15));
  std::cout << "Fast PWM tiers, continuous frame publication, key cadence and steady LED feedback passed\n";
}
