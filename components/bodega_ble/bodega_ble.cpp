#include "bodega_ble.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace esphome::bodega_ble {

static const char *const TAG = "bodega_ble";

static constexpr uint16_t BODEGA_SERVICE_UUID = 0x1234;
static constexpr uint16_t BODEGA_WRITE_UUID = 0x1235;
static constexpr uint16_t BODEGA_NOTIFY_UUID = 0x1236;

static constexpr uint8_t FRAME_HEADER = 0xFE;
static constexpr uint8_t COMMAND_BIND = 0x00;
static constexpr uint8_t COMMAND_QUERY = 0x01;
static constexpr uint8_t COMMAND_SET_OTHERS = 0x02;
static constexpr uint8_t COMMAND_RESET = 0x04;
static constexpr uint8_t COMMAND_SET_LEFT = 0x05;
static constexpr uint8_t COMMAND_SET_RIGHT = 0x06;
static constexpr uint8_t COMMAND_SET_HOT = 0x07;
static constexpr uint8_t COMMAND_CHECK_BIND = 0x08;
static constexpr uint8_t COMMAND_SET_BIND = 0x09;

static constexpr int EMPTY_DEGREE = -128;
static constexpr size_t MAX_FRAME_LENGTH = 64;

namespace {

std::string uppercase_copy(const std::string &value) {
  std::string upper = value;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char chr) {
    return static_cast<char>(std::toupper(chr));
  });
  return upper;
}

bool starts_with_(const std::string &value, const char *prefix) {
  return value.rfind(prefix, 0) == 0;
}

bool is_bodega_name_(const std::string &name) {
  const auto upper_name = uppercase_copy(name);
  return starts_with_(upper_name, "MF-") || starts_with_(upper_name, "A1-") ||
         starts_with_(upper_name, "WT-0001") || starts_with_(upper_name, "AK1-") ||
         starts_with_(upper_name, "AK2-") || starts_with_(upper_name, "AK3-") ||
         starts_with_(upper_name, "KGS-") || starts_with_(upper_name, "KGD-") ||
         starts_with_(upper_name, "W-") || starts_with_(upper_name, "WH-");
}

#ifdef USE_ESP32_BLE_DEVICE
std::vector<uint64_t> logged_discovery_addresses;

bool advertises_bodega_service_(const espbt::ESPBTDevice &device) {
  const auto bodega_uuid = esp32_ble::ESPBTUUID::from_uint16(BODEGA_SERVICE_UUID);

  for (const auto &uuid : device.get_service_uuids()) {
    if (uuid == bodega_uuid) {
      return true;
    }
  }

  for (const auto &service_data : device.get_service_datas()) {
    if (service_data.uuid == bodega_uuid) {
      return true;
    }
  }

  return false;
}

bool should_log_discovery_address_(uint64_t address) {
  if (std::find(logged_discovery_addresses.begin(), logged_discovery_addresses.end(), address) !=
      logged_discovery_addresses.end()) {
    return false;
  }
  logged_discovery_addresses.push_back(address);
  return true;
}
#endif

}  // namespace

static uint8_t sint_to_byte(int value) {
  value = std::clamp(value, -128, 127);
  return value < 0 ? static_cast<uint8_t>(value + 256) : static_cast<uint8_t>(value);
}

static int byte_to_sint(uint8_t value) { return value > 127 ? static_cast<int>(value) - 256 : static_cast<int>(value); }

static uint8_t unit_to_byte(TemperatureUnit unit) {
  return unit == FAHRENHEIT ? 1 : 0;
}

static TemperatureUnit unit_from_byte(uint8_t unit) {
  return unit == 1 ? FAHRENHEIT : CELSIUS;
}

static const char *unit_to_string(TemperatureUnit unit) {
  return unit == FAHRENHEIT ? "fahrenheit" : "celsius";
}

void BodegaBLE::setup() { this->apply_report_unit_to_entities_(); }

float BodegaBLE::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BodegaBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "Bodega BLE Fridge");
  if (this->parent() != nullptr) {
    ESP_LOGCONFIG(TAG, "  BLE client address: %s", this->parent()->address_str());
  }
  ESP_LOGCONFIG(TAG, "  Report Unit: %s", unit_to_string(this->report_unit_));
  ESP_LOGCONFIG(TAG, "  Bind On Connect: %s", YESNO(this->bind_on_connect_));
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Left Current Sensor", this->left_current_sensor_);
  LOG_SENSOR("  ", "Right Current Sensor", this->right_current_sensor_);
  LOG_SENSOR("  ", "Battery Percentage Sensor", this->battery_percentage_sensor_);
  LOG_SENSOR("  ", "Battery Voltage Sensor", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Model Sensor", this->model_sensor_);
  LOG_SENSOR("  ", "Run Mode Sensor", this->run_mode_sensor_);
  LOG_SENSOR("  ", "Battery Saver Sensor", this->battery_saver_sensor_);
  LOG_SENSOR("  ", "Running Status Sensor", this->running_status_sensor_);
  LOG_SENSOR("  ", "Warning Code Sensor", this->warning_code_sensor_);
  LOG_SENSOR("  ", "Sleep Mode Sensor", this->sleep_mode_sensor_);
  LOG_SENSOR("  ", "Partition Mode Sensor", this->partition_mode_sensor_);
  LOG_SENSOR("  ", "Disinfect Mode Sensor", this->disinfect_mode_sensor_);

  LOG_NUMBER("  ", "Left Target Number", this->left_target_number_);
  LOG_NUMBER("  ", "Right Target Number", this->right_target_number_);
  LOG_NUMBER("  ", "Hot Target Number", this->hot_target_number_);

  LOG_SWITCH("  ", "Power Switch", this->power_switch_);
  LOG_SWITCH("  ", "Lock Switch", this->lock_switch_);

  LOG_SELECT("  ", "Unit Select", this->unit_select_);

  LOG_BUTTON("  ", "Refresh Button", this->refresh_button_);
  LOG_BUTTON("  ", "Bind Button", this->bind_button_);
}

void BodegaBLE::set_report_unit(TemperatureUnit unit) {
  this->report_unit_ = unit;
  this->apply_report_unit_to_entities_();
  if (this->has_state_) {
    this->publish_all_states_();
  }
}

void BodegaBLE::set_bind_on_connect(bool bind_on_connect) { this->bind_on_connect_ = bind_on_connect; }

#ifdef USE_ESP32_BLE_DEVICE
bool BodegaBLE::parse_device(const espbt::ESPBTDevice &device) {
  const auto has_bodega_service = advertises_bodega_service_(device);
  const auto has_bodega_name = is_bodega_name_(device.get_name());
  if (!has_bodega_service && !has_bodega_name) {
    return false;
  }

  const auto address = device.address_uint64();
  if (!should_log_discovery_address_(address)) {
    return false;
  }

  const auto reason = has_bodega_service ? "advertised 1234 service" : "matching advertised name";
  if (device.get_name().empty()) {
    ESP_LOGI(TAG, "Discovered possible Bodega/Alpicool BLE fridge %s RSSI=%d (%s). Use this MAC in ble_client.mac_address.",
             device.address_str().c_str(), device.get_rssi(), reason);
  } else {
    ESP_LOGI(TAG,
             "Discovered possible Bodega/Alpicool BLE fridge %s RSSI=%d name='%s' (%s). Use this MAC in ble_client.mac_address.",
             device.address_str().c_str(), device.get_rssi(), device.get_name().c_str(), reason);
  }

  return false;
}
#endif

void BodegaBLE::update() {
  if (!this->ready_to_write_()) {
    return;
  }
  this->send_simple_command_(COMMAND_QUERY);
}

void BodegaBLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGD(TAG, "Connected to fridge BLE server");
      }
      break;
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT:
      this->reset_connection_state_();
      this->status_set_warning();
      ESP_LOGW(TAG, "Fridge BLE connection closed");
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->reset_connection_state_();

      auto *write_char = this->parent()->get_characteristic(BODEGA_SERVICE_UUID, BODEGA_WRITE_UUID);
      auto *notify_char = this->parent()->get_characteristic(BODEGA_SERVICE_UUID, BODEGA_NOTIFY_UUID);
      if (write_char == nullptr || notify_char == nullptr) {
        this->status_set_warning();
        ESP_LOGW(TAG, "Required Bodega service characteristics were not found");
        break;
      }

      this->write_handle_ = write_char->handle;
      this->notify_handle_ = notify_char->handle;
      this->write_props_ = write_char->properties;

      if ((this->write_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE) != 0) {
        this->write_type_ = ESP_GATT_WRITE_TYPE_RSP;
      } else if ((this->write_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) != 0) {
        this->write_type_ = ESP_GATT_WRITE_TYPE_NO_RSP;
      } else {
        this->status_set_warning();
        ESP_LOGW(TAG, "Bodega write characteristic does not support write operations");
        break;
      }

      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      this->notify_handle_);
      if (status != ESP_GATT_OK) {
        this->status_set_warning();
        ESP_LOGW(TAG, "Failed to register for Bodega notifications, status=%d", status);
        break;
      }

      ESP_LOGD(TAG, "Bodega characteristic handles discovered: write=0x%04X notify=0x%04X", this->write_handle_,
               this->notify_handle_);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.handle != this->notify_handle_) {
        break;
      }
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        this->status_set_warning();
        ESP_LOGW(TAG, "Notification registration failed, status=%d", param->reg_for_notify.status);
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->status_clear_warning();
      this->defer("bodega-post-connect", [this]() {
        if (this->bind_on_connect_) {
          this->request_bind();
        } else {
          this->update();
        }
      });
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle != this->notify_handle_) {
        break;
      }
      this->rx_buffer_.insert(this->rx_buffer_.end(), param->notify.value, param->notify.value + param->notify.value_len);
      this->process_rx_buffer_();
      break;
    case ESP_GATTC_WRITE_CHAR_EVT:
      if (param->write.handle == this->write_handle_ && param->write.status != ESP_GATT_OK) {
        this->status_momentary_warning("bodega-write", 3000);
        ESP_LOGW(TAG, "Characteristic write failed, status=%d", param->write.status);
      }
      break;
    default:
      break;
  }
}

bool BodegaBLE::set_left_target(float value) {
  if (!this->has_state_) {
    ESP_LOGW(TAG, "Cannot set left target before first fridge state is received");
    return false;
  }

  FridgeState next = this->state_;
  next.left_target = this->report_to_device_temperature_(value, false);
  if (!this->send_temperature_command_(COMMAND_SET_LEFT, next.left_target)) {
    return false;
  }

  this->state_ = next;
  this->publish_all_states_();
  this->schedule_query_(1200);
  return true;
}

bool BodegaBLE::set_right_target(float value) {
  if (!this->supports_right_zone_()) {
    ESP_LOGW(TAG, "Right target is not available for this fridge");
    return false;
  }

  FridgeState next = this->state_;
  next.right_target = this->report_to_device_temperature_(value, false);
  if (!this->send_temperature_command_(COMMAND_SET_RIGHT, next.right_target)) {
    return false;
  }

  this->state_ = next;
  this->publish_all_states_();
  this->schedule_query_(1200);
  return true;
}

bool BodegaBLE::set_hot_target(float value) {
  if (!this->supports_hot_target_()) {
    ESP_LOGW(TAG, "Hot target is not available for this fridge");
    return false;
  }

  FridgeState next = this->state_;
  next.hot_target = this->report_to_device_temperature_(value, false);
  if (!this->send_temperature_command_(COMMAND_SET_HOT, next.hot_target)) {
    return false;
  }

  this->state_ = next;
  this->publish_all_states_();
  this->schedule_query_(1200);
  return true;
}

bool BodegaBLE::set_power(bool powered_on) {
  if (!this->has_state_) {
    ESP_LOGW(TAG, "Cannot set power before first fridge state is received");
    return false;
  }

  FridgeState next = this->state_;
  next.powered_on = powered_on ? 1 : 0;
  if (!this->send_state_command_(next)) {
    return false;
  }

  this->state_ = next;
  this->publish_all_states_();
  this->schedule_query_(1200);
  return true;
}

bool BodegaBLE::set_lock(bool locked) {
  if (!this->has_state_) {
    ESP_LOGW(TAG, "Cannot set lock before first fridge state is received");
    return false;
  }

  FridgeState next = this->state_;
  next.locked = locked ? 1 : 0;
  if (!this->send_state_command_(next)) {
    return false;
  }

  this->state_ = next;
  this->publish_all_states_();
  this->schedule_query_(1200);
  return true;
}

bool BodegaBLE::set_device_unit(TemperatureUnit unit) {
  if (!this->has_state_) {
    ESP_LOGW(TAG, "Cannot set fridge unit before first fridge state is received");
    return false;
  }

  if (this->native_unit_() == unit) {
    this->publish_all_states_();
    return true;
  }

  FridgeState next = this->state_;
  this->convert_state_native_unit_(next, unit);
  if (!this->send_state_command_(next)) {
    return false;
  }

  this->state_ = next;
  this->publish_all_states_();
  this->schedule_query_(1200);
  return true;
}

void BodegaBLE::request_bind() {
  if (this->send_simple_command_(COMMAND_BIND)) {
    this->schedule_query_(800);
  }
}

bool BodegaBLE::send_simple_command_(uint8_t command) { return this->send_payload_(std::vector<uint8_t>{command}); }

bool BodegaBLE::send_temperature_command_(uint8_t command, int value) {
  std::vector<uint8_t> payload{command, sint_to_byte(value)};
  return this->send_payload_(payload);
}

bool BodegaBLE::send_state_command_(const FridgeState &state) {
  return this->send_payload_(this->encode_set_others_payload_(state));
}

bool BodegaBLE::send_payload_(const std::vector<uint8_t> &payload) {
  if (!this->ready_to_write_()) {
    this->status_momentary_warning("bodega-not-ready", 3000);
    ESP_LOGW(TAG, "Cannot send Bodega command before BLE notification setup completes");
    return false;
  }

  const auto frame = this->encode_frame_(payload);
  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->write_handle_,
                                         frame.size(), const_cast<uint8_t *>(frame.data()), this->write_type_,
                                         ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    this->status_momentary_warning("bodega-write", 3000);
    ESP_LOGW(TAG, "esp_ble_gattc_write_char failed, status=%d", status);
    return false;
  }
  return true;
}

std::vector<uint8_t> BodegaBLE::encode_frame_(const std::vector<uint8_t> &payload) const {
  std::vector<uint8_t> frame;
  frame.reserve(payload.size() + 5);
  frame.push_back(FRAME_HEADER);
  frame.push_back(FRAME_HEADER);
  frame.push_back(static_cast<uint8_t>(payload.size() + 2));
  frame.insert(frame.end(), payload.begin(), payload.end());

  uint16_t checksum = 0;
  for (uint8_t byte : frame) {
    checksum = static_cast<uint16_t>(checksum + byte);
  }
  frame.push_back(static_cast<uint8_t>((checksum >> 8) & 0xFF));
  frame.push_back(static_cast<uint8_t>(checksum & 0xFF));
  return frame;
}

std::vector<uint8_t> BodegaBLE::encode_set_others_payload_(const FridgeState &state) const {
  std::vector<uint8_t> payload;
  payload.reserve(40);

  auto push_signed = [&payload](int value) { payload.push_back(sint_to_byte(value)); };

  payload.push_back(COMMAND_SET_OTHERS);
  payload.push_back(state.locked);
  payload.push_back(state.powered_on);
  payload.push_back(state.run_mode);
  payload.push_back(state.battery_saver);
  push_signed(state.left_target);
  push_signed(state.temp_max);
  push_signed(state.temp_min);
  push_signed(state.left_return_diff);
  payload.push_back(state.start_delay);
  payload.push_back(state.unit);
  push_signed(state.left_tc_hot);
  push_signed(state.left_tc_mid);
  push_signed(state.left_tc_cold);
  push_signed(state.left_tc_halt);

  if (state.model != 1) {
    push_signed(state.right_target);
    push_signed(state.right_max);
    push_signed(state.right_min);
    push_signed(state.right_return_diff);
    push_signed(state.right_tc_hot);
    push_signed(state.right_tc_mid);
    push_signed(state.right_tc_cold);
    push_signed(state.right_tc_halt);
    push_signed(state.hot_target);
    payload.push_back(state.model);
  }

  if (state.model >= 9 || state.model == 1) {
    payload.push_back(state.running_status);
    payload.push_back(state.sleep_mode);
    payload.push_back(state.disinfect_mode);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
  } else {
    payload.push_back(0);
  }

  return payload;
}

void BodegaBLE::process_rx_buffer_() {
  while (this->rx_buffer_.size() >= 5) {
    if (this->rx_buffer_[0] != FRAME_HEADER || this->rx_buffer_[1] != FRAME_HEADER) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    const uint8_t length = this->rx_buffer_[2];
    if (length < 2 || length > MAX_FRAME_LENGTH) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    const size_t total_length = static_cast<size_t>(length) + 3;
    if (this->rx_buffer_.size() < total_length) {
      return;
    }

    uint16_t checksum = 0;
    for (size_t i = 0; i < total_length - 2; i++) {
      checksum = static_cast<uint16_t>(checksum + this->rx_buffer_[i]);
    }

    const uint16_t received_checksum =
        (static_cast<uint16_t>(this->rx_buffer_[total_length - 2]) << 8) | this->rx_buffer_[total_length - 1];
    if (checksum != received_checksum) {
      ESP_LOGW(TAG, "Dropping Bodega frame with invalid checksum");
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    std::vector<uint8_t> payload(this->rx_buffer_.begin() + 3, this->rx_buffer_.begin() + total_length - 2);
    this->process_payload_(payload);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + total_length);
  }
}

void BodegaBLE::process_payload_(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }

  this->status_clear_warning();

  switch (payload[0]) {
    case COMMAND_BIND:
      if (payload.size() > 1) {
        ESP_LOGD(TAG, "Bind response: %u", payload[1]);
      }
      break;
    case COMMAND_QUERY:
    case COMMAND_SET_OTHERS:
    case COMMAND_RESET: {
      FridgeState parsed;
      std::vector<uint8_t> data(payload.begin() + 1, payload.end());
      if (this->parse_state_from_data_(data, parsed)) {
        this->apply_state_(parsed);
      } else {
        ESP_LOGW(TAG, "Failed to parse fridge state payload");
      }
      break;
    }
    case COMMAND_SET_LEFT:
      if (payload.size() > 1 && this->has_state_) {
        this->state_.left_target = byte_to_sint(payload[1]);
        this->publish_all_states_();
      }
      break;
    case COMMAND_SET_RIGHT:
      if (payload.size() > 1 && this->has_state_) {
        this->state_.right_target = byte_to_sint(payload[1]);
        this->publish_all_states_();
      }
      break;
    case COMMAND_SET_HOT:
      if (payload.size() > 1 && this->has_state_) {
        this->state_.hot_target = byte_to_sint(payload[1]);
        this->publish_all_states_();
      }
      break;
    case COMMAND_CHECK_BIND:
      if (payload.size() > 1) {
        ESP_LOGD(TAG, "Check-bind response: %u", payload[1]);
      }
      break;
    case COMMAND_SET_BIND:
      ESP_LOGD(TAG, "Set-bind response received");
      break;
    default:
      ESP_LOGD(TAG, "Unhandled Bodega payload type: %u", payload[0]);
      break;
  }
}

bool BodegaBLE::parse_state_from_data_(const std::vector<uint8_t> &data, FridgeState &state) const {
  if (data.size() < 18) {
    return false;
  }

  state.locked = data[0];
  state.powered_on = data[1];
  state.run_mode = data[2];
  state.battery_saver = data[3];
  state.left_target = byte_to_sint(data[4]);
  state.temp_max = byte_to_sint(data[5]);
  state.temp_min = byte_to_sint(data[6]);
  state.left_return_diff = byte_to_sint(data[7]);
  state.start_delay = data[8];
  state.unit = data[9];
  state.left_tc_hot = byte_to_sint(data[10]);
  state.left_tc_mid = byte_to_sint(data[11]);
  state.left_tc_cold = byte_to_sint(data[12]);
  state.left_tc_halt = byte_to_sint(data[13]);
  state.left_current = byte_to_sint(data[14]);
  state.battery_percent = data[15];
  state.battery_voltage_int = data[16];
  state.battery_voltage_dec = data[17];

  if (data.size() > 25) {
    state.has_right_zone = true;
    state.right_target = byte_to_sint(data[18]);
    state.right_max = byte_to_sint(data[19]);
    state.right_min = byte_to_sint(data[20]);
    state.right_return_diff = byte_to_sint(data[21]);
    state.right_tc_hot = byte_to_sint(data[22]);
    state.right_tc_mid = byte_to_sint(data[23]);
    state.right_tc_cold = byte_to_sint(data[24]);
    state.right_tc_halt = byte_to_sint(data[25]);
  }

  if (data.size() > 26) {
    state.right_current = byte_to_sint(data[26]);
  } else {
    state.right_current = EMPTY_DEGREE;
  }

  if (data.size() > 27) {
    state.running_status = data[27];
  }

  if (data.size() > 28) {
    state.model = data[28];
  }

  if (data.size() > 29) {
    state.has_hot_target = true;
    state.hot_target = byte_to_sint(data[29]);
  }

  if (data.size() > 33) {
    state.has_advanced_fields = true;
    state.warning_code = data[30];
    state.sleep_mode = data[31];
    state.partition_mode = data[32];
    state.disinfect_mode = data[33];
  }

  return true;
}

void BodegaBLE::apply_state_(const FridgeState &state) {
  this->state_ = state;
  this->has_state_ = true;
  this->publish_all_states_();
}

void BodegaBLE::reset_connection_state_() {
  this->write_handle_ = 0;
  this->notify_handle_ = 0;
  this->write_props_ = 0;
  this->rx_buffer_.clear();
}

void BodegaBLE::schedule_query_(uint32_t delay_ms) {
  this->set_timeout("bodega-query", delay_ms, [this]() { this->update(); });
}

void BodegaBLE::publish_all_states_() {
  this->apply_report_unit_to_entities_();
  if (!this->has_state_) {
    return;
  }

  if (this->left_current_sensor_ != nullptr) {
    this->left_current_sensor_->publish_state(this->device_to_report_temperature_(this->state_.left_current, false));
  }

  if (this->right_current_sensor_ != nullptr) {
    if (this->supports_right_zone_() && this->state_.right_current != EMPTY_DEGREE) {
      this->right_current_sensor_->publish_state(this->device_to_report_temperature_(this->state_.right_current, false));
    } else {
      this->right_current_sensor_->publish_state(NAN);
    }
  }

  if (this->battery_percentage_sensor_ != nullptr) {
    this->battery_percentage_sensor_->publish_state(this->state_.battery_percent);
  }

  if (this->battery_voltage_sensor_ != nullptr) {
    const float voltage = this->state_.battery_voltage_int + (this->state_.battery_voltage_dec / 10.0f);
    this->battery_voltage_sensor_->publish_state(voltage);
  }

  if (this->model_sensor_ != nullptr && this->supports_right_zone_()) {
    this->model_sensor_->publish_state(this->state_.model);
  }

  if (this->run_mode_sensor_ != nullptr) {
    this->run_mode_sensor_->publish_state(this->state_.run_mode);
  }

  if (this->battery_saver_sensor_ != nullptr) {
    this->battery_saver_sensor_->publish_state(this->state_.battery_saver);
  }

  if (this->running_status_sensor_ != nullptr && this->supports_right_zone_()) {
    this->running_status_sensor_->publish_state(this->state_.running_status);
  }

  if (this->warning_code_sensor_ != nullptr && this->state_.has_advanced_fields) {
    this->warning_code_sensor_->publish_state(this->state_.warning_code);
  }

  if (this->sleep_mode_sensor_ != nullptr && this->state_.has_advanced_fields) {
    this->sleep_mode_sensor_->publish_state(this->state_.sleep_mode);
  }

  if (this->partition_mode_sensor_ != nullptr && this->state_.has_advanced_fields) {
    this->partition_mode_sensor_->publish_state(this->state_.partition_mode);
  }

  if (this->disinfect_mode_sensor_ != nullptr && this->state_.has_advanced_fields) {
    this->disinfect_mode_sensor_->publish_state(this->state_.disinfect_mode);
  }

  if (this->left_target_number_ != nullptr) {
    this->left_target_number_->publish_state(this->device_to_report_temperature_(this->state_.left_target, false));
  }

  if (this->right_target_number_ != nullptr && this->supports_right_zone_()) {
    this->right_target_number_->publish_state(this->device_to_report_temperature_(this->state_.right_target, false));
  }

  if (this->hot_target_number_ != nullptr && this->supports_hot_target_()) {
    this->hot_target_number_->publish_state(this->device_to_report_temperature_(this->state_.hot_target, false));
  }

  if (this->power_switch_ != nullptr) {
    this->power_switch_->publish_state(this->state_.powered_on != 0);
  }

  if (this->lock_switch_ != nullptr) {
    this->lock_switch_->publish_state(this->state_.locked != 0);
  }

  if (this->unit_select_ != nullptr) {
    this->unit_select_->publish_state(this->state_.unit == 1 ? 1 : 0);
  }
}

void BodegaBLE::apply_report_unit_to_entities_() {
  if (this->left_target_number_ != nullptr) {
    this->left_target_number_->traits.set_step(1);
    if (this->has_state_) {
      this->left_target_number_->traits.set_min_value(this->device_to_report_temperature_(this->state_.temp_min, false));
      this->left_target_number_->traits.set_max_value(this->device_to_report_temperature_(this->state_.temp_max, false));
    } else {
      this->left_target_number_->traits.set_min_value(-40);
      this->left_target_number_->traits.set_max_value(140);
    }
  }

  if (this->right_target_number_ != nullptr) {
    this->right_target_number_->traits.set_step(1);
    if (this->supports_right_zone_()) {
      this->right_target_number_->traits.set_min_value(this->device_to_report_temperature_(this->state_.right_min, false));
      this->right_target_number_->traits.set_max_value(this->device_to_report_temperature_(this->state_.right_max, false));
    } else {
      this->right_target_number_->traits.set_min_value(-40);
      this->right_target_number_->traits.set_max_value(140);
    }
  }

  if (this->hot_target_number_ != nullptr) {
    this->hot_target_number_->traits.set_step(1);
    this->hot_target_number_->traits.set_min_value(-40);
    this->hot_target_number_->traits.set_max_value(140);
  }
}

bool BodegaBLE::supports_right_zone_() const { return this->has_state_ && this->state_.has_right_zone; }

bool BodegaBLE::supports_hot_target_() const { return this->has_state_ && this->state_.has_hot_target; }

bool BodegaBLE::ready_to_write_() {
  return this->parent() != nullptr && this->node_state == espbt::ClientState::ESTABLISHED && this->write_handle_ != 0 &&
         this->notify_handle_ != 0;
}

TemperatureUnit BodegaBLE::native_unit_() const { return unit_from_byte(this->state_.unit); }

float BodegaBLE::convert_temperature_(float value, TemperatureUnit from, TemperatureUnit to, bool delta) const {
  if (from == to) {
    return value;
  }

  if (from == CELSIUS && to == FAHRENHEIT) {
    return std::lroundf(delta ? (value * 1.8f) : (value * 1.8f + 32.0f));
  }

  return std::lroundf(delta ? (value * 5.0f / 9.0f) : ((value - 32.0f) * 5.0f / 9.0f));
}

int BodegaBLE::convert_temperature_value_(int value, TemperatureUnit from, TemperatureUnit to, bool delta) const {
  if (value == EMPTY_DEGREE) {
    return value;
  }
  return std::clamp(static_cast<int>(this->convert_temperature_(value, from, to, delta)), -128, 127);
}

float BodegaBLE::device_to_report_temperature_(int value, bool delta) const {
  return this->convert_temperature_(value, this->native_unit_(), this->report_unit_, delta);
}

int BodegaBLE::report_to_device_temperature_(float value, bool delta) const {
  return std::clamp(static_cast<int>(this->convert_temperature_(value, this->report_unit_, this->native_unit_(), delta)),
                    -128, 127);
}

void BodegaBLE::convert_state_native_unit_(FridgeState &state, TemperatureUnit new_unit) const {
  const TemperatureUnit old_unit = unit_from_byte(state.unit);
  if (old_unit == new_unit) {
    state.unit = unit_to_byte(new_unit);
    return;
  }

  auto convert_absolute = [this, old_unit, new_unit](int &field) {
    field = this->convert_temperature_value_(field, old_unit, new_unit, false);
  };
  auto convert_delta = [this, old_unit, new_unit](int &field) {
    field = this->convert_temperature_value_(field, old_unit, new_unit, true);
  };

  convert_absolute(state.left_target);
  convert_absolute(state.temp_max);
  convert_absolute(state.temp_min);
  convert_delta(state.left_return_diff);
  convert_delta(state.left_tc_hot);
  convert_delta(state.left_tc_mid);
  convert_delta(state.left_tc_cold);
  convert_delta(state.left_tc_halt);
  convert_absolute(state.left_current);

  if (state.has_right_zone) {
    convert_absolute(state.right_target);
    convert_absolute(state.right_max);
    convert_absolute(state.right_min);
    convert_delta(state.right_return_diff);
    convert_delta(state.right_tc_hot);
    convert_delta(state.right_tc_mid);
    convert_delta(state.right_tc_cold);
    convert_delta(state.right_tc_halt);
    if (state.right_current != EMPTY_DEGREE) {
      convert_absolute(state.right_current);
    }
  }

  if (state.has_hot_target) {
    convert_absolute(state.hot_target);
  }

  state.unit = unit_to_byte(new_unit);
}

}  // namespace esphome::bodega_ble

#endif
