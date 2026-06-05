#pragma once

#include "bodega_ble.h"
#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

enum NumberKind : uint8_t {
  LEFT_TARGET = 0,
  RIGHT_TARGET = 1,
  HOT_TARGET = 2,
};

class BodegaBLENumber : public number::Number, public Parented<BodegaBLE> {
 public:
  explicit BodegaBLENumber(NumberKind kind) : kind_(kind) {}

 protected:
  void control(float value) override;

  NumberKind kind_;
};

}  // namespace esphome::bodega_ble

#endif
