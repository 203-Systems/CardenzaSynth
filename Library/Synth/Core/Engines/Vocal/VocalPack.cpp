// SPDX-License-Identifier: BSD-3-Clause
#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "Library/Memory/Memory.h"
#include "Library/Synth/Core/Engines/Vocal/VocalBuiltinPack.h"

namespace synth::vocal {

namespace {

// A name field: printable ASCII, at least one character, zero-filled to its end.
bool Text(const std::uint8_t* p, unsigned size)
{
  unsigned i = 0;
  for (; i < size && p[i] != 0; ++i)
    if (p[i] < 32 || p[i] > 126) return false;
  if (i == 0 || i == size) return false;
  for (; i < size; ++i)
    if (p[i] != 0) return false;
  return true;
}

bool Range(unsigned start, unsigned count, unsigned total)
{
  return count > 0 && start < total && count <= total - start;
}

}  // namespace

std::uint32_t Pack::Checksum(const std::uint8_t* bytes, std::size_t size)
{
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t index = 0; index < size; ++index)
  {
    crc ^= bytes[index];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

bool Pack::Validate(const std::uint8_t* b, std::size_t size)
{
  if (b == nullptr || size < kHeaderBytes || size > kMaxPackBytes || std::memcmp(b, "NPVOCAL\0", 8) != 0) return false;
  if (U16(b + 8) != kFormat || U16(b + 10) != kHeaderBytes || U32(b + 12) != size || U32(b + 16) != Checksum(b + 20, size - 20)) return false;
  for (unsigned offset : {32u, 64u, 96u})
    if (!Text(b + offset, 32)) return false;
  const unsigned waves = U16(b + 20), phones = U16(b + 22), zones = U16(b + 24), entries = U16(b + 26), segments = U16(b + 28);
  if (waves == 0 || waves > 256 || phones == 0 || phones > 256 || zones == 0 || zones > 1024 || entries == 0 || entries > kMaxEntries ||
      segments == 0 || segments > 2048) return false;
  const std::size_t po = kHeaderBytes + static_cast<std::size_t>(waves) * kWaveBytes;
  const std::size_t zo = po + static_cast<std::size_t>(phones) * kPhonemeBytes;
  const std::size_t eo = zo + static_cast<std::size_t>(zones) * kZoneBytes;
  const std::size_t so = eo + static_cast<std::size_t>(entries) * kEntryBytes;
  if (so + static_cast<std::size_t>(segments) * kSegmentBytes != size) return false;
  for (unsigned i = 0; i < waves; ++i)
  {
    const std::uint8_t* w = b + kHeaderBytes + i * kWaveBytes;
    // Six-bit samples, stored as (sample - 32) * 4.
    for (unsigned k = 0; k < kWaveSamples; ++k)
      if ((w[k] & 3) != 0) return false;
    if (w[field::kWaveVolume] > 32 || w[field::kWaveModDepth] > 63 || U16(w + field::kWaveModRate) > 4095 ||
        U16(w + field::kWaveModDelayMs) > 10000 || U16(w + 70) != 0) return false;
    for (unsigned j = field::kWaveModTable; j < kWaveBytes; ++j)
      if (w[j] > 7) return false;
  }
  for (unsigned i = 0; i < zones; ++i)
  {
    const std::uint8_t* z = b + zo + i * kZoneBytes;
    if (U16(z) >= waves || z[2] > 127 || z[3] > z[4] || z[4] > 127 || z[5] != 0 || U16(z + 6) != 0) return false;
  }
  for (unsigned i = 0; i < phones; ++i)
  {
    const std::uint8_t* p = b + po + i * kPhonemeBytes;
    if (!Range(U16(p), U16(p + 2), zones) || p[field::kPhonemePitchMode] > 2 ||
        (p[field::kPhonemePitchMode] == 2 && p[field::kPhonemePitchValue] > 127) || p[field::kPhonemeKind] != 1 ||
        U16(p + field::kPhonemeAttackMs) > 1000) return false;
  }
  bool foundDefault = false;
  unsigned previous = 0;
  for (unsigned i = 0; i < entries; ++i)
  {
    const std::uint8_t* e = b + eo + i * kEntryBytes;
    const unsigned id = U16(e), start = U16(e + 2), count = U16(e + 4);
    if ((i != 0 && id <= previous) || U16(e + 6) != 0 || !Text(e + 8, kEntryNameBytes) || !Range(start, count, segments) || count > 16)
      return false;
    previous = id;
    foundDefault = foundDefault || id == U16(b + 30);
    for (unsigned j = 0; j < count; ++j)
    {
      const std::uint8_t* s = b + so + (start + j) * kSegmentBytes;
      if (U16(s) >= phones || U16(s + field::kSegmentDurationMs) > 10000 || s[5] != 0 || U16(s + 6) > 1000 ||
          (U16(s + field::kSegmentDurationMs) == 0 && j + 1 != count)) return false;
    }
  }
  return foundDefault;
}

Pack::Pack(const std::uint8_t* data, std::size_t size, std::uint8_t* owned) : data_(data), size_(size), owned_(owned)
{
  phonemes_ = data_ + kHeaderBytes + static_cast<std::size_t>(WaveCount()) * kWaveBytes;
  zones_ = phonemes_ + static_cast<std::size_t>(PhonemeCount()) * kPhonemeBytes;
  entries_ = zones_ + static_cast<std::size_t>(ZoneCount()) * kZoneBytes;
  segments_ = entries_ + static_cast<std::size_t>(EntryCount()) * kEntryBytes;
}

Pack::~Pack()
{
  if (owned_ != nullptr) memory::FreeSlowArray(owned_);
}

PackRef Pack::Load(const std::uint8_t* bytes, std::size_t size)
{
  if (!Validate(bytes, size)) return {};
  // Read per note and per segment, never per sample (a voice copies the wave it plays): PSRAM.
  auto* copy = static_cast<std::uint8_t*>(memory::AllocateSlowArray(size));
  if (copy == nullptr) return {};
  std::memcpy(copy, bytes, size);
  Pack* pack = new (std::nothrow) Pack(copy, size, copy);
  if (pack == nullptr)
  {
    memory::FreeSlowArray(copy);
    return {};
  }
  return PackRef(pack);
}

PackRef Pack::Builtin()
{
  static const PackRef pack = [] {
    const bool valid = Validate(kBuiltinPack.data(), kBuiltinPack.size());
    Pack* built = valid ? new (std::nothrow) Pack(kBuiltinPack.data(), kBuiltinPack.size(), nullptr) : nullptr;
    return PackRef(built);
  }();
  return pack;
}

int Pack::Find(unsigned id) const
{
  unsigned low = 0, high = EntryCount();
  while (low < high)
  {
    const unsigned middle = (low + high) / 2;
    if (EntryId(middle) < id) low = middle + 1;
    else high = middle;
  }
  return low < EntryCount() && EntryId(low) == id ? static_cast<int>(low) : -1;
}

unsigned Pack::DefaultEntry() const
{
  const int index = Find(U16(data_ + 30));
  return index < 0 ? 0u : static_cast<unsigned>(index);
}

const std::uint8_t* Pack::SelectWave(unsigned phoneme, int note) const
{
  const std::uint8_t* p = Phoneme(phoneme);
  if (p[field::kPhonemePitchMode] == 1) note += Signed(p[field::kPhonemePitchValue]);
  else if (p[field::kPhonemePitchMode] == 2) note = p[field::kPhonemePitchValue];
  const std::uint8_t* best = zones_ + U16(p) * kZoneBytes;
  int distance = 100000;
  for (unsigned i = 0; i < U16(p + 2); ++i)
  {
    const std::uint8_t* z = zones_ + (U16(p) + i) * kZoneBytes;
    const int away = std::abs(note - static_cast<int>(z[2])) + ((note < z[3] || note > z[4]) ? 256 : 0);
    if (away < distance)
    {
      distance = away;
      best = z;
    }
  }
  return data_ + kHeaderBytes + static_cast<std::size_t>(U16(best)) * kWaveBytes;
}

}  // namespace synth::vocal
