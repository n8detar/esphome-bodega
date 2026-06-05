#include "bodega_ble_button.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::bodega_ble {

static const char *const TAG = "bodega_ble.button";

void BodegaBLEButton::press_action() {
  if (this->get_parent() == nullptr) {
    ESP_LOGW(TAG, "Ignoring button press without parent");
    return;
  }

  switch (this->kind_) {
    case REFRESH:
      this->get_parent()->update();
      break;
    case BIND:
      this->get_parent()->request_bind();
      break;
  }
}

}  // namespace esphome::bodega_ble

#endif
