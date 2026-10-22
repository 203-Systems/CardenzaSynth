#pragma once

#include <cstdint>

#include "Applications/Synth/UI/Model/SynthUiTypes.h"
#include "Devices/Device.h"

namespace synth::led {

inline constexpr std::uint8_t kAvailable = 1;
inline constexpr std::uint8_t kContent = 6;
inline constexpr std::uint8_t kFunction = 6;
inline constexpr std::uint8_t kRoot = 6;
inline constexpr std::uint8_t kSelected = 10;
inline constexpr std::uint8_t kPressed = Device::LED::kBrightnessMax;

inline std::uint8_t BarLevel(const UiBarItemSnapshot& item, const UiScreenSnapshot& screen)
{
  if ((item.label.empty() && item.icon == UiBarIcon::None) || item.disabled) return 0;
  if (item.pressed) return kPressed;
  if ((screen.samplerRecording && item.danger) || item.playing) return kPressed;
  return item.active ? kSelected : kFunction;
}

inline std::uint8_t KeyboardLevel(const UiScreenSnapshot& screen, std::size_t row, bool hinted, bool root)
{
  if (!hinted || screen.sequencer.patternSelectorVisible) return 0;
  if (screen.mode != UiMode::Home)
  {
    // Search/name entry gets one input rail; music pages keep root locators.
    if (screen.showPresetBrowser || screen.contentStyle == UiContentStyle::TextEntry)
      return row == kKeyboardMatrixRows - 1 ? kAvailable : 0;
    return root ? kAvailable : 0;
  }
  // In step view the pitch zone recedes until a step is selected for editing.
  if (screen.showSequencerGrid && !screen.sequencer.hasHeldSteps)
    return root ? kAvailable : 0;
  return root ? kRoot : kAvailable;
}

inline std::uint8_t EditValueLevel(const SequencerUiState& sequence, std::size_t row, std::size_t valueIndex)
{
  if (valueIndex >= sequence.editValueCount) return 0;
  if (!sequence.editValueMixed && valueIndex == sequence.editValueIndex) return kSelected;
  // The bottom row presents the choices; only the selected column lights above it.
  return row == kKeyboardMatrixRows - 1 ? kFunction : 0;
}

inline std::uint8_t StepLevel(const SequencerStepUiState& step, bool latched, bool copySource)
{
  if (step.playing) return kPressed;
  if (step.held) return latched ? kSelected : kPressed;
  if (copySource) return kSelected;
  return step.hasEvents ? kContent : kAvailable;
}

}  // namespace synth::led
