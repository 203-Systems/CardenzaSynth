// SPDX-License-Identifier: BSD-3-Clause
// A vocal track as the render thread plays it: the pack, the engine's parameters, the keys held and
// the voices (Library/Synth/Audio/SynthVocal.h says which word a note sings). Everything here runs on
// the render thread; nothing allocates.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "Library/Synth/Audio/SynthVocal.h"
#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"
#include "Library/Synth/Core/Engines/Vocal/VocalVoice.h"

namespace synth::vocal {

// One voice of a full-scale wave at full level and velocity, in the mix's units (a sampler voice's
// is 0.3): chosen so the built-in voice at its defaults sits with the other engines' presets.
inline constexpr float kVoiceGain = 0.22f;
// Keys held at once that the track remembers (a slur goes back to the one before when a key lets go).
inline constexpr std::size_t kHeldKeys = 8;
// A word key is for the notes that start with it: one that no note took up within this long (20 ms)
// is forgotten. A key and its note reach the track in the same instant, at most a block apart.
inline constexpr std::uint32_t kWordKeyFrames = 882;

class Track {
public:
  // Another singer: every voice stops (they point into the pack).
  void SetPack(PackRef pack)
  {
    for (Voice& voice : voices_) voice.Stop();
    heldCount_ = 0;
    pack_ = std::move(pack);
    keyWord_ = -1;
    singing_ = -1;
  }
  const Pack* CurrentPack() const { return pack_.get(); }

  // An engine parameter ("amp.level", "voice.word", ...). False for an id this engine does not have.
  bool SetParameter(std::string_view id, int value)
  {
    if (id == "voice.word") word_ = std::clamp(value, 0, static_cast<int>(kMaxEntries) - 1);
    else if (id == "amp.level") level_ = static_cast<float>(std::clamp(value, 0, 127)) * (1.0f / 127.0f);
    else if (id == "env.attack") settings_.attackMs = TimeMs(value);
    else if (id == "env.release") settings_.releaseMs = TimeMs(value);
    else if (id == "voice.glide") settings_.glideFrames = Frames(TimeMs(value));
    else if (id == "vib.depth") settings_.vibratoDepth = VibratoSemitones(value);
    else if (id == "vib.rate") settings_.vibratoStep = VibratoHz(value) * static_cast<float>(kControlFrames) / kVoiceSampleRate;
    else if (id == "vib.delay") settings_.vibratoDelayFrames = Frames(VibratoDelayMs(value));
    else if (id == "voice.mode") poly_ = value != 0;
    else if (id == "voice.tune")
    {
      settings_.transpose = std::clamp(value, -24, 24);
      for (Voice& voice : voices_) voice.Retune(settings_);
    }
    else return false;
    return true;
  }

  // A word key (a note below kWordKeyCount; its velocity, 1..127, says which word): the note that
  // starts with it sings that word, and notes held now change to it at once. It sounds nothing.
  void WordKey(std::uint8_t note, std::uint8_t velocity)
  {
    const int word = WordOfKey(note, velocity);
    if (!pack_ || word < 0 || static_cast<unsigned>(word) >= pack_->EntryCount()) return;  // a word the singer does not have
    if (HeldVoice() == nullptr)
    {
      // For the note that comes with it.
      keyWord_ = word;
      keyFrames_ = kWordKeyFrames;
      return;
    }
    keyWord_ = -1;
    // The word the held notes already sing goes on undisturbed: a step sends its key with every note
    // it starts, the later notes of a chord and the later hits of a note too.
    if (static_cast<unsigned>(word) == groupEntry_) return;
    for (Voice& voice : voices_)
    {
      if (!voice.Held()) continue;
      // A note that started in this very instant (a step holds both) starts over with the word; one
      // already sounding changes to it.
      if (voice.Age() == 0) voice.Start(*pack_, static_cast<unsigned>(word), voice.Note(), voice.Velocity(), settings_);
      else (void)voice.Pronounce(static_cast<unsigned>(word), settings_);
    }
    groupEntry_ = static_cast<unsigned>(word);
    singing_ = static_cast<std::int16_t>(word);
    ++onsets_;
  }

  // `velocity` is 0..1.
  void NoteOn(std::uint8_t note, float velocity)
  {
    if (!pack_ || IsWordKey(note)) return;
    (void)RemoveHeld(note);
    const bool joins = heldCount_ > 0;
    PushHeld(note, velocity);
    if (joins)
    {
      if (poly_)
      {
        // A chord: another voice on the word the first key started.
        StartVoice(groupEntry_, note, velocity);
        return;
      }
      if (Voice* voice = HeldVoice())
      {
        // A slur: the word goes on at the new pitch.
        voice->Tie(note, settings_);
        return;
      }
      // The word ended by itself (it has no part that holds) with its key still down: a new one.
    }
    // The word its key said, or the track's own.
    const int word = keyWord_ >= 0 ? keyWord_ : word_;
    keyWord_ = -1;
    groupEntry_ = static_cast<unsigned>(word) < pack_->EntryCount() ? static_cast<unsigned>(word) : pack_->DefaultEntry();
    singing_ = static_cast<std::int16_t>(groupEntry_);
    StartVoice(groupEntry_, note, velocity);
    ++onsets_;
  }

  void NoteOff(std::uint8_t note)
  {
    if (IsWordKey(note) || !RemoveHeld(note)) return;
    if (poly_)
    {
      for (Voice& voice : voices_)
        if (voice.Held() && voice.Note() == note) voice.Release(settings_);
      return;
    }
    Voice* voice = HeldVoice();
    if (voice == nullptr || voice->Note() != note) return;  // an earlier key of the slur: nothing changes
    if (heldCount_ > 0) voice->Tie(held_[heldCount_ - 1].note, settings_);  // back to the key still down
    else voice->Release(settings_);
  }

  void AllNotesOff()
  {
    heldCount_ = 0;
    keyWord_ = -1;
    for (Voice& voice : voices_) voice.Release(settings_);
  }

  // Adds the voices to `out`, at most a block.
  void Render(float* out, std::size_t frames)
  {
    for (Voice& voice : voices_)
      if (voice.Active()) voice.Render(out, frames, settings_, kVoiceGain * level_ * (0.2f + 0.8f * voice.Velocity()));
  }

  // Time passing, `frames` of it, whether or not the track sounds (a silent track is not rendered):
  // a word key nobody took up runs out, and nothing is being sung once no note is held.
  void Tick(std::size_t frames)
  {
    if (singing_ >= 0 && HeldVoice() == nullptr) singing_ = -1;
    if (keyWord_ < 0) return;
    if (keyFrames_ <= frames) keyWord_ = -1;
    else keyFrames_ -= static_cast<std::uint32_t>(frames);
  }

  // A voice sounds (a let-go tail too).
  bool Active() const
  {
    for (const Voice& voice : voices_)
      if (voice.Active()) return true;
    return false;
  }
  // A key is down.
  bool Held() const { return heldCount_ > 0; }
  // Marks the keys held, one bit per MIDI note.
  void AddHeldNotes(std::array<std::uint32_t, 4>& notes) const
  {
    for (std::size_t index = 0; index < heldCount_; ++index) notes[held_[index].note / 32] |= 1u << (held_[index].note % 32);
  }
  Status CurrentStatus() const { return Status{singing_, onsets_}; }
  const Voice& VoiceAt(std::size_t index) const { return voices_[index]; }

private:
  struct HeldKey {
    std::uint8_t note = 0;
    float velocity = 0.0f;
  };

  Voice* HeldVoice()
  {
    for (Voice& voice : voices_)
      if (voice.Held()) return &voice;
    return nullptr;
  }

  void StartVoice(unsigned entry, std::uint8_t note, float velocity)
  {
    // A free voice; else the oldest let-go one; a held one only when every voice holds a note.
    std::size_t chosen = 0;
    int best = -1;
    for (std::size_t index = 0; index < voices_.size(); ++index)
    {
      const Voice& voice = voices_[index];
      const int rank = !voice.Active() ? 3 : (voice.Releasing() ? 2 : 1);
      if (rank > best || (rank == best && serials_[index] < serials_[chosen]))
      {
        best = rank;
        chosen = index;
      }
    }
    serials_[chosen] = ++serial_;
    voices_[chosen].Start(*pack_, entry, note, velocity, settings_);
  }

  void PushHeld(std::uint8_t note, float velocity)
  {
    if (heldCount_ == held_.size())
    {
      for (std::size_t index = 1; index < held_.size(); ++index) held_[index - 1] = held_[index];
      --heldCount_;
    }
    held_[heldCount_++] = HeldKey{note, velocity};
  }

  bool RemoveHeld(std::uint8_t note)
  {
    for (std::size_t index = 0; index < heldCount_; ++index)
    {
      if (held_[index].note != note) continue;
      for (std::size_t next = index + 1; next < heldCount_; ++next) held_[next - 1] = held_[next];
      --heldCount_;
      return true;
    }
    return false;
  }

  PackRef pack_;
  VoiceSettings settings_{};
  float level_ = 96.0f / 127.0f;
  bool poly_ = false;
  int word_ = 0;                  // the track's own word ("voice.word")
  int keyWord_ = -1;              // the word a key named for the note that comes with it, -1 for none
  std::uint32_t keyFrames_ = 0;   // how long that key still counts
  std::int16_t singing_ = -1;
  std::uint32_t onsets_ = 0;
  unsigned groupEntry_ = 0;
  std::array<HeldKey, kHeldKeys> held_{};
  std::size_t heldCount_ = 0;
  std::array<Voice, kVoiceCount> voices_{};
  std::array<std::uint32_t, kVoiceCount> serials_{};
  std::uint32_t serial_ = 0;
};

}  // namespace synth::vocal
