#include "bodega_ble_switch.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

static const char *const TAG = "bodega_ble.switch";

void BodegaBLESwitch::write_state(bool state) {
  if (this->get_parent() == nullptr) {
    ESP_LOGW(TAG, "Ignoring switch write without parent");
    return;
  }

  bool ok = false;
  switch (this->kind_) {
    case POWER:
      ok = this->get_parent()->set_power(state);
      break;
    case LOCK:
      ok = this->get_parent()->set_lock(state);
      break;
  }

  if (!ok) {
    ESP_LOGW(TAG, "Failed to write switch state");
  }
}

}  // namespace esphome::bodega_ble

#endif
