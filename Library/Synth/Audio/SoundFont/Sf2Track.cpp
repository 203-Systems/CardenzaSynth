#include "Library/Synth/Audio/SoundFont/Sf2Track.h"

namespace synth::audio::sf2 {

void Track::SetProgram(const Program* program)
{
  for (Voice& voice : voices_) voice.Stop();
  program_ = program;
}

void Track::NoteOn(unsigned key, unsigned velocity)
{
  if (key > 127) return;
  if (velocity == 0)
  {
    NoteOff(key);
    return;
  }
  if (program_ == nullptr) return;
  const Region* layers[kMaxLayers];
  const std::size_t count = program_->Find(key, velocity > 127 ? 127 : velocity, layers, kMaxLayers);
  if (count == 0) return;

  // The same key again: the note it played goes quickly, as a struck string is damped.
  for (Voice& voice : voices_)
    if (voice.Active() && !voice.Fading() && voice.Key() == key) voice.Choke(kQuickFadeSteps);
  // An exclusive class: every other note of it ends (an open hi-hat closing).
  for (std::size_t layer = 0; layer < count; ++layer)
  {
    const unsigned exclusive = layers[layer]->exclusiveClass;
    if (exclusive == 0) continue;
    for (Voice& voice : voices_)
      if (voice.Active() && !voice.Fading() && voice.ExclusiveClass() == exclusive) voice.Choke(kQuickFadeSteps);
  }

  ++order_;
  for (std::size_t layer = 0; layer < count; ++layer)
  {
    Voice& voice = VoiceForNewNote();
    const std::int16_t* data = program_->SlotData(layers[layer]->slot);
    voice.StartAfterFade(*layers[layer], data, key, velocity > 127 ? 127 : velocity, tone_, order_);
  }
}

// A free voice; else one already on its way out; else the oldest let go; else the oldest held (a
// note lost, counted). A voice that waits to start a note keeps it unless nothing else is left.
Voice& Track::VoiceForNewNote()
{
  Voice* best = nullptr;
  int bestRank = 0;
  for (Voice& voice : voices_)
  {
    int rank;
    if (!voice.Active()) return voice;
    if (voice.Waiting()) rank = 0;
    else if (voice.Fading()) rank = 4;
    else if (!voice.Held()) rank = 3;
    else rank = 2;
    if (best == nullptr || rank > bestRank || (rank == bestRank && voice.Order() < best->Order()))
    {
      best = &voice;
      bestRank = rank;
    }
  }
  if (best->Held() && !best->Fading()) ++stolenHeld_;
  return *best;
}

void Track::NoteOff(unsigned key)
{
  for (Voice& voice : voices_)
    if (voice.Held() && voice.Key() == key) voice.Release();
}

void Track::AllNotesOff()
{
  for (Voice& voice : voices_)
    if (voice.Held()) voice.Release();
}

void Track::Silence()
{
  for (Voice& voice : voices_) voice.Choke(1);
}

void Track::Render(std::int32_t* accumulator, std::size_t frames)
{
  for (Voice& voice : voices_)
    if (voice.Active()) voice.Render(accumulator, frames);
}

bool Track::Busy() const
{
  for (const Voice& voice : voices_)
    if (voice.Active()) return true;
  return false;
}

std::size_t Track::SoundingVoices() const
{
  std::size_t count = 0;
  for (const Voice& voice : voices_) count += voice.Active() ? 1 : 0;
  return count;
}

std::size_t Track::FadingVoices() const
{
  std::size_t count = 0;
  for (const Voice& voice : voices_) count += voice.Active() && voice.Fading() ? 1 : 0;
  return count;
}

Track::Faded Track::FadeVoice()
{
  Voice* chosen = nullptr;
  for (Voice& voice : voices_)
  {
    if (!voice.Active() || voice.Fading()) continue;
    // A tail before a held note; of two alike, the quieter.
    if (chosen == nullptr || (chosen->Held() && !voice.Held()) || (chosen->Held() == voice.Held() && voice.Gain() < chosen->Gain())) chosen = &voice;
  }
  if (chosen == nullptr) return Faded::None;
  const Faded faded = chosen->Held() ? Faded::Held : Faded::Tail;
  chosen->Choke(kBudgetFadeSteps);
  return faded;
}

bool Track::Holds(unsigned key) const
{
  for (const Voice& voice : voices_)
    if (voice.Held() && voice.Key() == key) return true;
  return false;
}

bool Track::HoldsNotes() const
{
  for (const Voice& voice : voices_)
    if (voice.Held()) return true;
  return false;
}

void Track::AddHeldNotes(std::array<std::uint32_t, 4>& notes) const
{
  for (const Voice& voice : voices_)
    if (voice.Held()) notes[voice.Key() / 32] |= 1u << (voice.Key() % 32);
}

}  // namespace synth::audio::sf2
