#pragma once

#include "bodega_ble.h"
#include "esphome/components/select/select.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

enum SelectKind : uint8_t {
  UNIT = 0,
};

class BodegaBLESelect : public select::Select, public Parented<BodegaBLE> {
 public:
  explicit BodegaBLESelect(SelectKind kind) : kind_(kind) {}

 protected:
  void control(size_t index) override;

  SelectKind kind_;
};

}  // namespace esphome::bodega_ble

#endif
