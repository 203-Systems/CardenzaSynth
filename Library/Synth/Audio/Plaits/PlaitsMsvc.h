#pragma once

// Put in front of the Plaits sources by MSVC builds (cardenza_collect_plaits_cpp_sources, /FI): upstream
// is written for GCC and is not edited here.
#if defined(_MSC_VER)
// M_PI from <cmath> (stmlib/dsp/filter.h, cosine_oscillator.h, plaits/dsp/oscillator/sine_oscillator.h).
#if !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif
// stmlib/utils/dsp.h marks its inline functions __attribute__((always_inline)).
#define __attribute__(x)
#endif
