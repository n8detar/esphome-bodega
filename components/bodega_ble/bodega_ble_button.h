#pragma once

#include "bodega_ble.h"
#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

enum ButtonKind : uint8_t {
  REFRESH = 0,
  BIND = 1,
};

class BodegaBLEButton : public button::Button, public Parented<BodegaBLE> {
 public:
  explicit BodegaBLEButton(ButtonKind kind) : kind_(kind) {}

 protected:
  void press_action() override;

  ButtonKind kind_;
};

}  // namespace esphome::bodega_ble

#endif
