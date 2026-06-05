#include "bodega_ble_number.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

static const char *const TAG = "bodega_ble.number";

void BodegaBLENumber::control(float value) {
  if (this->get_parent() == nullptr) {
    ESP_LOGW(TAG, "Ignoring number write without parent");
    return;
  }

  bool ok = false;
  switch (this->kind_) {
    case LEFT_TARGET:
      ok = this->get_parent()->set_left_target(value);
      break;
    case RIGHT_TARGET:
      ok = this->get_parent()->set_right_target(value);
      break;
    case HOT_TARGET:
      ok = this->get_parent()->set_hot_target(value);
      break;
  }

  if (!ok) {
    ESP_LOGW(TAG, "Failed to write number state");
  }
}

}  // namespace esphome::bodega_ble

#endif
