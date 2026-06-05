#include "bodega_ble_select.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

static const char *const TAG = "bodega_ble.select";

void BodegaBLESelect::control(size_t index) {
  if (this->get_parent() == nullptr) {
    ESP_LOGW(TAG, "Ignoring select write without parent");
    return;
  }

  bool ok = false;
  switch (this->kind_) {
    case UNIT:
      ok = this->get_parent()->set_device_unit(index == 1 ? FAHRENHEIT : CELSIUS);
      break;
  }

  if (!ok) {
    ESP_LOGW(TAG, "Failed to write select state");
  }
}

}  // namespace esphome::bodega_ble

#endif
