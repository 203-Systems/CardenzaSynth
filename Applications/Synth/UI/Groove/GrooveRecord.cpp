// Recording in SEQ: the OK key, the two note views, step recording, the RECORD panel and capture.
//
// OK (ENTER) on the SEQ note page: a tap switches the view (PIANO ROLL <-> STEP REC), a hold opens
// the RECORD panel. With steps selected it keeps cycling the edit lane. Capture keeps every note
// the pads play as takes, without recording; the RECORD panel puts one bar of a take into a new
// pattern (OK) or merges it into the playing one (FN + OK).
#include "Applications/Synth/UI/Groove/GrooveController.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace groove {

namespace {

constexpr std::uint32_t kPulsesPerStep = synth::kSequencerPulsesPerStep;
constexpr std::uint32_t kPulsesPerMicrostep = synth::kSequencerPulsesPerMicrostep;
constexpr std::uint32_t kPulsesPerQuarter = kPulsesPerStep * 4;

const char* ViewName(SeqView view)
{
  return view == SeqView::Roll ? "PIANO ROLL" : "STEP REC";
}

// The notes on a step, in order: what STEP REC compares with the pads.
std::size_t SortedStepNotes(const synth::SynthSequencer& sequencer, std::size_t track, std::size_t step, std::array<std::uint8_t, 8>& notes)
{
  std::array<synth::SynthSequencer::StepNoteView, 8> views{};
  const std::size_t count = sequencer.StepNoteEvents(track, step, views);
  for (std::size_t index = 0; index < count; ++index) notes[index] = views[index].note;
  std::sort(notes.begin(), notes.begin() + static_cast<std::ptrdiff_t>(count));
  return count;
}

struct TimedNote {
  std::uint32_t start = 0;  // pulses from the take's origin
  std::uint32_t length = 0;
};

float MsPerPulse(const CaptureTake& take)
{
  return 60000.0f / (static_cast<float>(std::max<std::uint16_t>(take.bpm, 1)) * kPulsesPerQuarter);
}

std::uint16_t FirstNoteMs(const RecordState& rec, const CaptureTake& take)
{
  std::uint16_t firstMs = 0xFFFF;
  for (std::size_t index = 0; index < take.count; ++index) firstMs = std::min(firstMs, rec.TakeNote(take, index).startMs);
  return firstMs;
}

// A note of a take on the sequencer's time line. Synced takes keep their place against the bar; free
// takes start with their first note, timed at the tempo they were played in.
TimedNote TimeOf(const CaptureTake& take, const CaptureNote& note, std::uint16_t firstMs, float msPerPulse)
{
  TimedNote timed;
  const std::uint32_t heldMs = note.lengthMs == 0 ? take.endedAt - take.startedAt - note.startMs : note.lengthMs;
  if (take.synced)
  {
    timed.start = note.startPulse;
    timed.length = note.lengthPulses != 0 ? note.lengthPulses : static_cast<std::uint32_t>(std::lround(static_cast<float>(heldMs) / msPerPulse));
  }
  else
  {
    timed.start = static_cast<std::uint32_t>(std::lround(static_cast<float>(note.startMs - firstMs) / msPerPulse));
    timed.length = static_cast<std::uint32_t>(std::lround(static_cast<float>(heldMs) / msPerPulse));
  }
  timed.length = std::max<std::uint32_t>(timed.length, kPulsesPerMicrostep);
  return timed;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Capture

void Controller::CaptureNoteOn(std::size_t track, std::uint8_t note, std::uint8_t velocity)
{
  if (note > 127 || track >= kTrackCount) return;
  RecordState& rec = state_.record;
  auto& sequencer = host_.Sequencer();
  const std::uint32_t now = state_.now;
  CaptureTake* take = rec.newest < kCaptureTakes ? &rec.takes[rec.newest] : nullptr;
  bool held = false;
  for (const std::uint16_t open : rec.openNote) held = held || open != 0;
  const bool fresh = take == nullptr || take->track != track || take->count >= kCaptureNotes ||
                     (!held && now - rec.lastPadActivity > kCaptureGapMs) || now - take->startedAt > kCaptureMaxMs;
  if (fresh)
  {
    rec.newest = rec.newest >= kCaptureTakes ? 0 : (rec.newest + 1) % kCaptureTakes;
    take = &rec.takes[rec.newest];
    *take = CaptureTake{};
    take->id = rec.nextTakeId++;
    take->track = static_cast<std::uint8_t>(track);
    take->startedAt = now;
    take->endedAt = now;
    take->bpm = sequencer.Tempo();
    take->synced = sequencer.Playing();
    const std::uint64_t pulse = sequencer.PlayPulse();
    take->basePulse = pulse - pulse % synth::kSongBarPulses;
    rec.openNote.fill(0);
    // The RECORD panel keeps pointing at the newest take while new ones arrive.
    if (rec.focus >= kRecordFirstTake)
    {
      rec.focus = static_cast<std::uint8_t>(kRecordFirstTake);
      rec.takeBar = 0;
    }
  }
  // The transport stopped during a synced take: time the rest of it by the clock.
  if (take->synced && !sequencer.Playing()) take->synced = false;
  // The takes share the notes: the oldest ones go when the newest need the room.
  if (take->count == 0) take->first = rec.nextNote;
  for (CaptureTake& older : rec.takes)
    if (&older != take && older.count != 0 && rec.nextNote - older.first >= kCaptureNotes) older = CaptureTake{};
  CaptureNote& captured = rec.notes[rec.nextNote++ % kCaptureNotes];
  captured = CaptureNote{};
  captured.note = note;
  captured.velocity = velocity;
  captured.startMs = static_cast<std::uint16_t>(std::min<std::uint32_t>(now - take->startedAt, 0xFFFF));
  if (take->synced)
    captured.startPulse = static_cast<std::uint16_t>(std::min<std::uint64_t>(sequencer.PlayPulse() - take->basePulse, 0xFFFF));
  rec.openNote[note] = ++take->count;
  rec.lastPadActivity = now;
  take->endedAt = now;
}

void Controller::CaptureNoteOff(std::size_t track, std::uint8_t note)
{
  RecordState& rec = state_.record;
  if (note > 127 || rec.newest >= kCaptureTakes) return;
  CaptureTake& take = rec.takes[rec.newest];
  const std::uint16_t open = rec.openNote[note];
  if (take.track != track || open == 0 || open > take.count) return;
  rec.openNote[note] = 0;
  CaptureNote& captured = rec.notes[(take.first + open - 1) % kCaptureNotes];
  const std::uint32_t now = state_.now;
  captured.lengthMs = static_cast<std::uint16_t>(std::clamp<std::uint32_t>(now - take.startedAt - captured.startMs, 1, 0xFFFF));
  auto& sequencer = host_.Sequencer();
  if (take.synced && sequencer.Playing())
  {
    const std::uint64_t pulse = sequencer.PlayPulse() - take.basePulse;
    captured.lengthPulses = static_cast<std::uint16_t>(std::clamp<std::uint64_t>(pulse - captured.startPulse, 1, 0xFFFF));
  }
  rec.lastPadActivity = now;
  take.endedAt = now;
}

std::size_t Controller::TakeCount() const
{
  std::size_t count = 0;
  for (const CaptureTake& take : state_.record.takes) count += take.id != 0 && take.count > 0 ? 1 : 0;
  return count;
}

const CaptureTake* Controller::TakeAt(std::size_t order) const
{
  const RecordState& rec = state_.record;
  if (rec.newest >= kCaptureTakes) return nullptr;
  std::size_t seen = 0;
  for (std::size_t back = 0; back < kCaptureTakes; ++back)
  {
    const CaptureTake& take = rec.takes[(rec.newest + kCaptureTakes - back) % kCaptureTakes];
    if (take.id == 0 || take.count == 0) continue;
    if (seen++ == order) return &take;
  }
  return nullptr;
}

std::size_t Controller::TakeBars(const CaptureTake& take) const
{
  const RecordState& rec = state_.record;
  const std::uint16_t firstMs = FirstNoteMs(rec, take);
  const float msPerPulse = MsPerPulse(take);
  const std::uint32_t window = static_cast<std::uint32_t>(host_.Sequencer().TrackStepCount(take.track)) * kPulsesPerStep;
  std::uint32_t last = 0;
  for (std::size_t index = 0; index < take.count; ++index) last = std::max(last, TimeOf(take, rec.TakeNote(take, index), firstMs, msPerPulse).start);
  return std::clamp<std::size_t>(window == 0 ? 1 : last / window + 1, 1, synth::kSequencerMaxPatternCount);
}

void Controller::ForEachTakeBarNote(const CaptureTake& take, std::size_t bar, void (*visit)(const PlacedNote&, void*), void* context) const
{
  const RecordState& rec = state_.record;
  const std::uint16_t firstMs = FirstNoteMs(rec, take);
  const float msPerPulse = MsPerPulse(take);
  const std::uint32_t steps = static_cast<std::uint32_t>(host_.Sequencer().TrackStepCount(take.track));
  const std::uint32_t window = steps * kPulsesPerStep;
  const std::uint32_t from = static_cast<std::uint32_t>(bar) * window;
  for (std::size_t index = 0; index < take.count; ++index)
  {
    const CaptureNote& captured = rec.TakeNote(take, index);
    const TimedNote note = TimeOf(take, captured, firstMs, msPerPulse);
    if (note.start < from || note.start >= from + window) continue;
    const std::uint32_t at = note.start - from;
    std::uint32_t step = at / kPulsesPerStep;
    std::uint32_t microstep = ((at % kPulsesPerStep) + kPulsesPerMicrostep / 2) / kPulsesPerMicrostep;
    if (state_.record.snap)
    {
      step = (at + kPulsesPerStep / 2) / kPulsesPerStep;
      microstep = 0;
    }
    if (microstep >= synth::kSequencerMicrostepCount)
    {
      ++step;
      microstep = 0;
    }
    if (step >= steps) continue;  // rounds into the next bar
    PlacedNote out;
    out.step = static_cast<std::uint8_t>(step);
    out.microstep = static_cast<std::uint8_t>(microstep);
    out.note = captured.note;
    out.velocity = captured.velocity;
    out.lengthPulses = static_cast<std::uint16_t>(std::clamp<std::uint32_t>(
        state_.record.snap ? std::max<std::uint32_t>(kPulsesPerStep, (note.length + kPulsesPerStep / 2) / kPulsesPerStep * kPulsesPerStep)
                           : note.length,
        1, 0xFFFF));
    visit(out, context);
  }
}

bool Controller::PlaceTake(bool merge)
{
  RecordState& rec = state_.record;
  const CaptureTake* take = rec.focus >= kRecordFirstTake ? TakeAt(rec.focus - kRecordFirstTake) : nullptr;
  if (take == nullptr)
  {
    ShowToast("PICK A TAKE", ToastKind::Error);
    return false;
  }
  const std::size_t bars = TakeBars(*take);
  const std::size_t bar = std::min<std::size_t>(rec.takeBar, bars - 1);
  std::size_t count = 0;
  ForEachTakeBarNote(*take, bar, [](const PlacedNote&, void* context) { ++*static_cast<std::size_t*>(context); }, &count);
  if (count == 0)
  {
    ShowToast("NO NOTES IN THIS BAR", ToastKind::Error);
    return false;
  }
  auto& sequencer = host_.Sequencer();
  const std::size_t track = take->track;
  std::size_t pattern = sequencer.ActivePattern(track);
  if (!merge)
  {
    // A new pattern: what is there already is never touched.
    const std::size_t patterns = sequencer.PatternCount(track);
    if (patterns >= synth::kSequencerMaxPatternCount || !sequencer.QueuePattern(track, patterns))
    {
      ShowToast("NO FREE PATTERN: FN+OK MERGES", ToastKind::Error);
      return false;
    }
    pattern = patterns;
  }
  struct Placing {
    synth::SynthSequencer& sequencer;
    std::size_t track;
    std::size_t pattern;
    std::size_t written;
  } placing{sequencer, track, pattern, 0};
  ForEachTakeBarNote(*take, bar, [](const PlacedNote& note, void* context) {
    auto& target = *static_cast<Placing*>(context);
    target.written += target.sequencer.InsertNote(target.track, target.pattern, note.step, note.microstep, note.note, note.velocity, note.lengthPulses) ? 1 : 0;
  }, &placing);
  const std::size_t written = placing.written;
  MarkDirty();
  const std::uint32_t id = take->id;
  CloseModal();
  SelectTrack(track);
  // The value line is drawn large: the pattern only; the note count goes with the title.
  ShowHud("TAKE " + std::to_string(id) + (bars > 1 ? " BAR " + std::to_string(bar + 1) : "") + "  " + std::to_string(written) +
              " NOTES",
          std::string(merge ? "MERGED P" : "NEW P") + (pattern + 1 < 10 ? "0" : "") + std::to_string(pattern + 1));
  return written > 0;
}

// ---------------------------------------------------------------------------------------------
// OK key, views and step recording

bool Controller::OkDeferred() const
{
  return state_.scene == Scene::Sequence && CurrentPage() == 0 && TopModal() == nullptr && !DrumLayout() &&
         !host_.Sequencer().BuildUiState(state_.track).hasHeldSteps;
}

void Controller::OkPress()
{
  RecordState& rec = state_.record;
  rec.okDown = true;
  rec.okHeld = false;
  rec.okDownAt = state_.now;
}

void Controller::OkRelease()
{
  RecordState& rec = state_.record;
  if (!rec.okDown) return;
  rec.okDown = false;
  if (!rec.okHeld) ToggleSeqView();
  rec.okHeld = false;
}

void Controller::OkTick()
{
  RecordState& rec = state_.record;
  if (!rec.okDown || rec.okHeld || state_.now - rec.okDownAt < kOkHoldMs) return;
  rec.okHeld = true;
  OpenRecord();
}

void Controller::ToggleSeqView()
{
  if (DrumLayout()) return;
  RecordState& rec = state_.record;
  rec.view = rec.view == SeqView::Roll ? SeqView::Step : SeqView::Roll;
  rec.stepWritten = false;
  rec.stepBeforeKnown = false;
  rec.cursorTapped = false;
  host_.Sequencer().ClearSelectedSteps();
  const std::size_t steps = host_.Sequencer().TrackStepCount(state_.track);
  if (rec.cursor >= steps) rec.cursor = 0;
  // The cursor comes to the page on the GRID, never writes out of sight.
  const std::size_t first = host_.Sequencer().ViewFirstStep(state_.track);
  if (rec.cursor / kGridCount != first / kGridCount) rec.cursor = static_cast<std::uint8_t>(first);
  ShowHud("VIEW", ViewName(rec.view));
}

void Controller::StepCursorMove(int delta)
{
  RecordState& rec = state_.record;
  if (delta != 0) rec.cursorTapped = false;
  const int steps = static_cast<int>(std::max<std::size_t>(1, host_.Sequencer().TrackStepCount(state_.track)));
  rec.cursor = static_cast<std::uint8_t>(((static_cast<int>(rec.cursor) + delta) % steps + steps) % steps);
  // The GRID stays on the page with the cursor.
  auto& sequencer = host_.Sequencer();
  if (sequencer.ViewFirstStep(state_.track) / kGridCount != rec.cursor / kGridCount) sequencer.SetViewPage(state_.track, rec.cursor / kGridCount);
}

bool Controller::StepRecordPress()
{
  std::array<std::uint8_t, kRows * kPadColumns> notes{};
  const std::size_t count = CollectHeldNotes(notes);
  if (count == 0) return false;
  RecordState& rec = state_.record;
  rec.cursorTapped = false;
  StepCursorMove(0);
  auto& sequencer = host_.Sequencer();
  // The first write of a press to a step remembers what was there: the same notes again erase it.
  if (!rec.stepWritten || !rec.stepBeforeKnown || rec.stepBeforeAt != rec.cursor)
  {
    rec.stepBeforeCount = static_cast<std::uint8_t>(SortedStepNotes(sequencer, state_.track, rec.cursor, rec.stepBefore));
    rec.stepBeforeAt = rec.cursor;
    rec.stepBeforeKnown = true;
  }
  if (!sequencer.SetStepNotes(state_.track, rec.cursor, notes.data(), count, state_.keyboard.velocity)) return false;
  rec.stepWritten = true;
  MarkDirty();
  return true;
}

void Controller::StepRecordRelease()
{
  RecordState& rec = state_.record;
  if (!rec.stepWritten) return;
  for (std::size_t index = 0; index < state_.padNote.size(); ++index)
    if (state_.padNote[index] != kNoNote && state_.padTrack[index] == state_.track) return;
  rec.stepWritten = false;
  // The notes that were there, pressed again: the step is erased (a rest), as a pad on a selected
  // step takes its note out in the piano roll.
  if (StepRecordRepeats()) EraseCursorStep();
  rec.stepBeforeKnown = false;
  StepCursorMove(rec.stepLength);
}

// The cursor step holds just the notes it had before the pads now down wrote there.
bool Controller::StepRecordRepeats() const
{
  const RecordState& rec = state_.record;
  if (!rec.stepBeforeKnown || rec.stepBeforeAt != rec.cursor || rec.stepBeforeCount == 0) return false;
  std::array<std::uint8_t, 8> now{};
  const std::size_t count = SortedStepNotes(host_.Sequencer(), state_.track, rec.cursor, now);
  return count == rec.stepBeforeCount && std::equal(now.begin(), now.begin() + static_cast<std::ptrdiff_t>(count), rec.stepBefore.begin());
}

// STEP REC: the cursor step loses its notes (FN + X, or its notes pressed again).
void Controller::EraseCursorStep()
{
  const std::uint8_t cursor = state_.record.cursor;
  std::array<std::uint8_t, 8> notes{};
  const bool hadNotes = SortedStepNotes(host_.Sequencer(), state_.track, cursor, notes) != 0;
  if (hadNotes && host_.Sequencer().SetStepNotes(state_.track, cursor, nullptr, 0, 0)) MarkDirty();
  ShowToast("STEP " + std::to_string(cursor + 1) + (hadNotes ? " CLEARED" : " EMPTY"));
}

// STEP REC, GRID tapped: the cursor goes there (the step is chosen), and the chosen step tapped again
// loses its notes, a recording taken back (as a tap on a hit takes it out on the drum page). Held pads
// write to that step at once, or erase it when it holds just their notes.
void Controller::StepRecordGrid(std::size_t step, std::size_t heldNotes)
{
  RecordState& rec = state_.record;
  if (step >= host_.Sequencer().TrackStepCount(state_.track)) return;
  if (heldNotes == 0)
  {
    const bool again = rec.cursorTapped && rec.cursorTappedTrack == state_.track && rec.cursor == step;
    rec.cursor = static_cast<std::uint8_t>(step);
    rec.cursorTapped = true;
    rec.cursorTappedTrack = static_cast<std::uint8_t>(state_.track);
    if (again) EraseCursorStep();
    return;
  }
  rec.cursor = static_cast<std::uint8_t>(step);
  rec.stepBeforeKnown = false;  // this tap decides for this step on its own
  if (!StepRecordPress()) return;
  if (StepRecordRepeats()) EraseCursorStep();
  rec.stepBeforeKnown = false;  // decided: letting go of the pads only moves the cursor on
  StepRecordRelease();
}

// ---------------------------------------------------------------------------------------------
// RECORD panel

void Controller::OpenRecord()
{
  Modal modal;
  modal.kind = ModalKind::Record;
  modal.title = "RECORD";
  RecordState& rec = state_.record;
  rec.focus = TakeCount() > 0 ? static_cast<std::uint8_t>(kRecordFirstTake) : static_cast<std::uint8_t>(kRecordRec);
  rec.takeBar = 0;
  OpenModal(std::move(modal));
}

void Controller::RecordGrid(std::size_t index)
{
  RecordState& rec = state_.record;
  if (index >= kRecordFirstTake + kCaptureTakes) return;
  const bool again = rec.focus == index;
  rec.focus = static_cast<std::uint8_t>(index);
  switch (index)
  {
    case kRecordRec: ToggleRecord(); break;
    case kRecordSnap: rec.snap = !rec.snap; break;
    case kRecordStepLength: rec.stepLength = rec.stepLength == 1 ? 2 : rec.stepLength == 2 ? 4 : 1; break;
    case kRecordView: ToggleSeqView(); break;
    default:
      if (!again) rec.takeBar = 0;
      break;
  }
}

void Controller::RecordAdjust(int delta)
{
  RecordState& rec = state_.record;
  auto& sequencer = host_.Sequencer();
  switch (rec.focus)
  {
    case kRecordRec:
      if ((delta > 0) != sequencer.RecordArmed()) ToggleRecord();
      break;
    case kRecordSnap: rec.snap = delta > 0; break;
    case kRecordStepLength:
      rec.stepLength = delta > 0 ? (rec.stepLength >= 4 ? 4 : rec.stepLength * 2) : (rec.stepLength <= 1 ? 1 : rec.stepLength / 2);
      break;
    case kRecordView:
      if ((delta > 0) != (rec.view == SeqView::Step)) ToggleSeqView();
      break;
    default:
    {
      const CaptureTake* take = TakeAt(rec.focus - kRecordFirstTake);
      if (take == nullptr) break;
      const int bars = static_cast<int>(TakeBars(*take));
      rec.takeBar = static_cast<std::uint8_t>(std::clamp(static_cast<int>(rec.takeBar) + delta, 0, bars - 1));
      break;
    }
  }
}

void Controller::RecordEnter()
{
  const RecordState& rec = state_.record;
  if (rec.focus >= kRecordFirstTake) (void)PlaceTake(false);
  else RecordGrid(rec.focus);
}

void Controller::RecordFnEnter()
{
  if (state_.record.focus >= kRecordFirstTake) (void)PlaceTake(true);
}

}  // namespace groove
