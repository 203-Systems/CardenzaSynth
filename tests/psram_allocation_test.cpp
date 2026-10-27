#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace {
constexpr std::size_t kPsramBytes = 2 * 1024 * 1024;
std::unordered_map<void*, std::size_t> allocations;
std::size_t used = 0;
std::size_t attempts = 0;
bool fail = false;
int destroyed = 0;

struct Owner {
  memory::ExternalArray<std::int16_t> samples;
  ~Owner() { ++destroyed; }
};
}

void* heap_caps_malloc(std::size_t bytes, std::uint32_t caps)
{
  ++attempts;
  assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (fail || bytes > kPsramBytes - used) return nullptr;
  void* value = std::malloc(bytes);
  assert(value != nullptr);
  allocations.emplace(value, bytes);
  used += bytes;
  return value;
}

void* heap_caps_calloc(std::size_t count, std::size_t bytes, std::uint32_t caps)
{
  assert(count <= std::numeric_limits<std::size_t>::max() / bytes);
  void* value = heap_caps_malloc(count * bytes, caps);
  if (value != nullptr) std::memset(value, 0, count * bytes);
  return value;
}

void heap_caps_free(void* pointer)
{
  if (pointer == nullptr) return;
  const auto found = allocations.find(pointer);
  assert(found != allocations.end());
  used -= found->second;
  allocations.erase(found);
  std::free(pointer);
}

int main()
{
  // The firmware's actual allocator path: a full five-second sample goes to
  // PSRAM, including a zeroed tail, and shared publication keeps it alive.
  {
    auto sample = synth::sampler::AllocateFrames(synth::sampler::kMaxSampleFrames);
    assert(sample && sample->frameCount == 44100 * 5);
    assert(allocations.at(sample->frames.get()) == 44100 * 5 * sizeof(std::int16_t));
    assert(sample->frames[0] == 0 && sample->frames[sample->frameCount - 1] == 0);
    sample->frames[17] = 1234;
    synth::audio::SharedSampleBuffer published = sample;
    sample.reset();
    assert(used != 0 && published->frames[17] == 1234);
  }
  assert(used == 0);

  {
    std::array<std::shared_ptr<synth::audio::SampleBuffer>, 5> samples;
    for (std::size_t track = 0; track < 4; ++track) {
      samples[track] = synth::sampler::AllocateFrames(synth::sampler::kMaxSampleFrames);
      assert(samples[track]);
    }
    samples[4] = synth::sampler::AllocateFrames(synth::sampler::kMaxSampleFrames);
    assert(!samples[4]);
  }
  assert(used == 0 && allocations.empty());

  // Replacing/freeing a PSRAM object must run its destructor and free nested
  // arrays with the matching allocator.
  {
    auto owner = memory::MakeExternal<Owner>();
    assert(owner);
    owner->samples = memory::MakeExternalArray<std::int16_t>(1024);
    assert(owner->samples);
    auto replacement = std::move(owner);
    assert(!owner && replacement);
  }
  assert(destroyed == 1 && used == 0 && allocations.empty());

  // A full/absent PSRAM pool must report failure without falling back to the
  // internal heap. Invalid lengths must never reach the allocator.
  fail = true;
  const std::size_t before = attempts;
  assert(!synth::sampler::AllocateFrames(128));
  assert(attempts == before + 1);
  assert(!memory::MakeExternal<Owner>());
  const std::size_t after = attempts;
  assert(!synth::sampler::AllocateFrames(0));
  assert(!synth::sampler::AllocateFrames(synth::sampler::kMaxSampleFrames + 1));
  assert(!memory::MakeExternalArray<std::int16_t>(std::numeric_limits<std::size_t>::max()));
  assert(attempts == after && used == 0 && allocations.empty());
}
