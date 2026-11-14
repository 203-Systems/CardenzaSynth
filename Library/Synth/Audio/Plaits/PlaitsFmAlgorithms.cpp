// Upstream's plaits/dsp/fm/algorithms.cc (the 6-operator models' routing tables), built under
// another object name.
//
// ESP-IDF's linker script generator finds objects by base name, and Devices/Cardenza/audio_hot.lf
// places functions of AMY's `algorithms` (its algorithms.c) in IRAM. A second algorithms.* object in
// the firmware's archive stops the build there ("Multiple matches for object"). The upstream file
// itself is not touched (Library/third_party/eurorack/README.md), and is not in the source list
// (cardenza_collect_plaits_cpp_sources): this file is.
#include "plaits/dsp/fm/algorithms.cc"
