// The guide's side of the controller: the hint lights (ComputeLeds), where a lesson starts, and the
// state a lesson page checks its steps against. GrooveApp carries the messages (docs/groovebox-design.md,
// 教程模式).

#include "Applications/Synth/UI/Groove/GrooveController.h"

#include <cstdio>
#include <string>

namespace groove {

void Controller::SetGuide(bool on)
{
  guide_ = on;
  if (!on) guideHint_ = 0;
}

void Controller::GuideGoTo(Scene scene, int page, std::size_t track)
{
  state_.modals.clear();
  ReleaseAllPads();
  SelectTrack(track);
  GoToScene(scene);
  if (page >= 0) SetPage(static_cast<std::uint8_t>(page));
}

std::string Controller::GuideState() const
{
  const auto& sequencer = host_.Sequencer();
  const std::size_t track = state_.track;
  const std::size_t pattern = sequencer.ActivePattern(track);
  // Steps of the current pattern that hold notes (hits on a drum track).
  std::size_t written = 0;
  std::array<synth::SynthSequencer::StepNoteView, 8> notes{};
  const std::size_t steps = sequencer.PatternStepCount(track, pattern);
  for (std::size_t step = 0; step < steps; ++step)
    if (sequencer.StepNoteEvents(track, step, notes) > 0) ++written;
  const Modal* modal = TopModal();
  const int overlay = EscOverlay() ? 1 : TabOverlay() ? 2 : FnOverlay() ? 3 : CtrlOverlay() ? 4 : 0;
  // The preset as a number that changes with it.
  unsigned preset = 0;
  if (const auto* engine = host_.Engine(track))
    for (const char c : engine->CurrentPresetId()) preset = (preset * 31u + static_cast<unsigned char>(c)) % 1000u;
  // The SONG cell under the cursor: its pattern number (P01 = 1), 0 for -- or past the song.
  unsigned songCell = 0;
  if (state_.songRow < sequencer.SongLength() && state_.songTrack < kTrackCount)
    songCell = static_cast<unsigned>(sequencer.SongRowAt(state_.songRow).patterns[state_.songTrack] + 1);
  char text[96];
  std::snprintf(text, sizeof(text), "s%up%ut%ue%uy%um%uo%uh%uq%uw%uk%uz%uc%ua%ug%ul%ux%uv%u", static_cast<unsigned>(state_.scene),  // scene: 0 SETUP .. 3 TAPE
                static_cast<unsigned>(CurrentPage()),                                                    // page of the scene, from 0
                static_cast<unsigned>(track),                                                            // track, from 0
                static_cast<unsigned>(host_.EngineKind(track)),                                          // its engine
                sequencer.Playing() ? 1u : 0u,                                                           // transport
                modal == nullptr ? 0u : modal->kind == ModalKind::Menu ? 1u : 2u,                        // FN menu kept open / another pop-up
                static_cast<unsigned>(overlay),                                                          // hold map: ESC TAB FN CTRL
                static_cast<unsigned>(written),                                                          // steps with notes
                static_cast<unsigned>(sequencer.PatternCount(track)),                                    // patterns of the track
                static_cast<unsigned>(pattern),                                                          // the one it plays
                static_cast<unsigned>(state_.keyboard.scale),                                            // scale (kScales)
                static_cast<unsigned>(state_.keyboard.layout),                                           // keyboard layout
                state_.chords[track].chord != ChordType::Off ? 1u : 0u,                                  // the track's chord
                sequencer.GetArpSettings(track).mode != synth::ArpMode::Off ? 1u : 0u,                   // its arpeggiator
                static_cast<unsigned>(sequencer.SongLength()),                                           // song rows
                sequencer.SongMode() ? 1u : 0u,                                                          // SONG or LOOP
                songCell, preset);
  return std::string(text) + "n" + PageTab(state_.scene, CurrentPage());
}

}  // namespace groove
