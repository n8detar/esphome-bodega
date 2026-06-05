#pragma once

#include "bodega_ble.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

enum SwitchKind : uint8_t {
  POWER = 0,
  LOCK = 1,
};

class BodegaBLESwitch : public switch_::Switch, public Parented<BodegaBLE> {
 public:
  explicit BodegaBLESwitch(SwitchKind kind) : kind_(kind) {}

 protected:
  void write_state(bool state) override;

  SwitchKind kind_;
};

}  // namespace esphome::bodega_ble

#endif
