// CardenzaSynth: this file replaces upstream's plaits/user_data.h (see ../README.md).
//
// Upstream reads user wavetables, wave terrains and DX7 banks from a page of the STM32's flash
// and writes them there. This firmware keeps nothing in internal flash, so there is no user data:
// every engine plays its built-in tables (plaits::Voice falls back to them when ptr() is null).

#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib/stmlib.h"

namespace plaits {

class UserData {
 public:
  UserData() { }
  ~UserData() { }

  inline const uint8_t* ptr(int slot) const {
    (void)slot;
    return NULL;
  }
};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_
