#include "Devices/PcmInputRing.h"

#include <array>
#include <deque>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

void Boundaries()
{
  device::audio::PcmInputRing<7> ring;
  const std::array<std::int16_t, 7> samples{-32768, -1, 0, 1, 32767, 42, -42};
  std::array<std::int16_t, 10> out;
  out.fill(123);
  Require(ring.Write(nullptr, 0) && !ring.Write(nullptr, 1), "invalid write handling");
  Require(ring.Read(out.data(), out.size()) == 0 && out[0] == 123, "empty read wrote data");
  Require(ring.Write(samples.data(), samples.size()), "full capacity write failed");
  Require(ring.Size() == 7 && !ring.Write(samples.data(), 1), "overflow was accepted");
  Require(!ring.Write(samples.data(), std::numeric_limits<std::size_t>::max()), "huge write was accepted");
  Require(ring.Read(nullptr, 3) == 0 && ring.Read(out.data(), 0) == 0 && ring.Size() == 7,
          "invalid read consumed samples");
  Require(ring.Read(out.data(), 5) == 5, "partial read failed");
  Require(std::equal(samples.begin(), samples.begin() + 5, out.begin()), "signed PCM changed");
  // Tail starts at zero; filling the ring then reading crosses its physical end.
  Require(ring.Write(samples.data(), 5), "wrapped refill failed");
  Require(ring.Read(out.data(), out.size()) == 7, "wrapped read length incorrect");
  const std::array<std::int16_t, 7> expected{42, -42, -32768, -1, 0, 1, 32767};
  Require(std::equal(expected.begin(), expected.end(), out.begin()) && out[7] == 123,
          "wrapped read order or bounds incorrect");

  // Write straddles the end; a rejected block must leave the old samples intact.
  Require(ring.Write(samples.data(), 6), "split write failed");
  Require(!ring.Write(samples.data(), 2) && ring.Size() == 6, "overflow partially wrote data");
  Require(ring.Read(out.data(), 10) == 6 && std::equal(samples.begin(), samples.begin() + 6, out.begin()),
          "overflow damaged pending data");
  Require(ring.Write(samples.data(), 4), "write before reset failed");
  ring.Reset();
  Require(ring.Size() == 0 && ring.Read(out.data(), 4) == 0, "reset retained old samples");
}

template<std::size_t Capacity>
void RandomTransfers()
{
  device::audio::PcmInputRing<Capacity> ring;
  std::deque<std::int16_t> reference;
  std::array<std::int16_t, Capacity + 5> samples{}, out{};
  std::mt19937 random(45678);
  for (int iteration = 0; iteration < 10000; ++iteration)
  {
    const std::size_t count = random() % samples.size();
    if (random() & 1)
    {
      for (auto& sample : samples) sample = static_cast<std::int16_t>(random());
      const bool fits = count <= Capacity - reference.size();
      Require(ring.Write(samples.data(), count) == fits, "write capacity check differs from reference");
      if (fits) reference.insert(reference.end(), samples.begin(), samples.begin() + count);
    }
    else
    {
      out.fill(1234);
      const std::size_t expected = std::min(count, reference.size());
      Require(ring.Read(out.data(), count) == expected, "read count differs from reference");
      for (std::size_t i = 0; i < expected; ++i)
      {
        Require(out[i] == reference.front(), "PCM order differs from reference");
        reference.pop_front();
      }
      for (std::size_t i = expected; i < out.size(); ++i)
        Require(out[i] == 1234, "read wrote beyond returned samples");
    }
    Require(ring.Size() == reference.size(), "ring occupancy differs from reference");
  }
}

void RecordingBlocks()
{
  device::audio::PcmInputRing<16384> ring;
  std::array<std::int16_t, 256> samples{}, out{};
  for (int block = 0; block < 200; ++block)
  {
    for (std::size_t i = 0; i < samples.size(); ++i) samples[i] = block * 256 + i;
    Require(ring.Write(samples.data(), samples.size()), "recording block write failed");
    Require(ring.Read(out.data(), out.size()) == samples.size() && out == samples,
            "recording block changed across ring wrap");
  }
}
}  // namespace

int main()
{
  try
  {
    Boundaries();
    RandomTransfers<7>();
    RandomTransfers<256>();
    RecordingBlocks();
    std::cout << "PCM input ring tests passed\n";
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
