#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/button/button.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <cstdint>
#include <vector>

namespace esphome::bodega_ble {

enum TemperatureUnit : uint8_t {
  CELSIUS = 0,
  FAHRENHEIT = 1,
};

struct FridgeState {
  uint8_t locked{0};
  uint8_t powered_on{1};
  uint8_t run_mode{0};
  uint8_t battery_saver{0};
  int left_target{0};
  int temp_max{20};
  int temp_min{-20};
  int left_return_diff{2};
  uint8_t start_delay{0};
  uint8_t unit{0};
  int left_tc_hot{0};
  int left_tc_mid{0};
  int left_tc_cold{-4};
  int left_tc_halt{0};
  int left_current{0};
  uint8_t battery_percent{50};
  uint8_t battery_voltage_int{0};
  uint8_t battery_voltage_dec{0};
  int right_target{0};
  int right_max{0};
  int right_min{0};
  int right_return_diff{2};
  int right_tc_hot{0};
  int right_tc_mid{0};
  int right_tc_cold{-4};
  int right_tc_halt{0};
  int right_current{-128};
  uint8_t running_status{0};
  uint8_t model{1};
  int hot_target{40};
  uint8_t warning_code{0};
  uint8_t sleep_mode{0};
  uint8_t partition_mode{0};
  uint8_t disinfect_mode{0};
  bool has_right_zone{false};
  bool has_hot_target{false};
  bool has_advanced_fields{false};
};

namespace espbt = esphome::esp32_ble_tracker;

class BodegaBLE : public ble_client::BLEClientNode, public PollingComponent, public espbt::ESPBTDeviceListener {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
#ifdef USE_ESP32_BLE_DEVICE
  bool parse_device(const espbt::ESPBTDevice &device) override;
#endif

  void set_report_unit(TemperatureUnit unit);
  void set_bind_on_connect(bool bind_on_connect);

  bool set_left_target(float value);
  bool set_right_target(float value);
  bool set_hot_target(float value);
  bool set_power(bool powered_on);
  bool set_lock(bool locked);
  bool set_device_unit(TemperatureUnit unit);
  void request_bind();

  void set_left_current_sensor(sensor::Sensor *sensor) { this->left_current_sensor_ = sensor; }
  void set_right_current_sensor(sensor::Sensor *sensor) { this->right_current_sensor_ = sensor; }
  void set_battery_percentage_sensor(sensor::Sensor *sensor) { this->battery_percentage_sensor_ = sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { this->battery_voltage_sensor_ = sensor; }
  void set_model_sensor(sensor::Sensor *sensor) { this->model_sensor_ = sensor; }
  void set_run_mode_sensor(sensor::Sensor *sensor) { this->run_mode_sensor_ = sensor; }
  void set_battery_saver_sensor(sensor::Sensor *sensor) { this->battery_saver_sensor_ = sensor; }
  void set_running_status_sensor(sensor::Sensor *sensor) { this->running_status_sensor_ = sensor; }
  void set_warning_code_sensor(sensor::Sensor *sensor) { this->warning_code_sensor_ = sensor; }
  void set_sleep_mode_sensor(sensor::Sensor *sensor) { this->sleep_mode_sensor_ = sensor; }
  void set_partition_mode_sensor(sensor::Sensor *sensor) { this->partition_mode_sensor_ = sensor; }
  void set_disinfect_mode_sensor(sensor::Sensor *sensor) { this->disinfect_mode_sensor_ = sensor; }

  void set_left_target_number(number::Number *number) { this->left_target_number_ = number; }
  void set_right_target_number(number::Number *number) { this->right_target_number_ = number; }
  void set_hot_target_number(number::Number *number) { this->hot_target_number_ = number; }

  void set_power_switch(switch_::Switch *power_switch) { this->power_switch_ = power_switch; }
  void set_lock_switch(switch_::Switch *lock_switch) { this->lock_switch_ = lock_switch; }

  void set_unit_select(select::Select *unit_select) { this->unit_select_ = unit_select; }

  void set_refresh_button(button::Button *refresh_button) { this->refresh_button_ = refresh_button; }
  void set_bind_button(button::Button *bind_button) { this->bind_button_ = bind_button; }

 protected:
  bool send_simple_command_(uint8_t command);
  bool send_temperature_command_(uint8_t command, int value);
  bool send_state_command_(const FridgeState &state);
  bool send_payload_(const std::vector<uint8_t> &payload);
  std::vector<uint8_t> encode_frame_(const std::vector<uint8_t> &payload) const;
  std::vector<uint8_t> encode_set_others_payload_(const FridgeState &state) const;

  void process_rx_buffer_();
  void process_payload_(const std::vector<uint8_t> &payload);
  bool parse_state_from_data_(const std::vector<uint8_t> &data, FridgeState &state) const;
  void apply_state_(const FridgeState &state);
  void reset_connection_state_();
  void schedule_query_(uint32_t delay_ms = 0);
  void publish_all_states_();
  void apply_report_unit_to_entities_();

  bool supports_right_zone_() const;
  bool supports_hot_target_() const;
  bool ready_to_write_();

  TemperatureUnit native_unit_() const;
  float convert_temperature_(float value, TemperatureUnit from, TemperatureUnit to, bool delta) const;
  int convert_temperature_value_(int value, TemperatureUnit from, TemperatureUnit to, bool delta) const;
  float device_to_report_temperature_(int value, bool delta) const;
  int report_to_device_temperature_(float value, bool delta) const;
  void convert_state_native_unit_(FridgeState &state, TemperatureUnit new_unit) const;

  TemperatureUnit report_unit_{CELSIUS};
  bool bind_on_connect_{false};
  bool has_state_{false};
  FridgeState state_{};

  uint16_t write_handle_{0};
  uint16_t notify_handle_{0};
  esp_gatt_char_prop_t write_props_{0};
  esp_gatt_write_type_t write_type_{ESP_GATT_WRITE_TYPE_RSP};
  std::vector<uint8_t> rx_buffer_;

  sensor::Sensor *left_current_sensor_{nullptr};
  sensor::Sensor *right_current_sensor_{nullptr};
  sensor::Sensor *battery_percentage_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *model_sensor_{nullptr};
  sensor::Sensor *run_mode_sensor_{nullptr};
  sensor::Sensor *battery_saver_sensor_{nullptr};
  sensor::Sensor *running_status_sensor_{nullptr};
  sensor::Sensor *warning_code_sensor_{nullptr};
  sensor::Sensor *sleep_mode_sensor_{nullptr};
  sensor::Sensor *partition_mode_sensor_{nullptr};
  sensor::Sensor *disinfect_mode_sensor_{nullptr};

  number::Number *left_target_number_{nullptr};
  number::Number *right_target_number_{nullptr};
  number::Number *hot_target_number_{nullptr};

  switch_::Switch *power_switch_{nullptr};
  switch_::Switch *lock_switch_{nullptr};

  select::Select *unit_select_{nullptr};

  button::Button *refresh_button_{nullptr};
  button::Button *bind_button_{nullptr};
};

}  // namespace esphome::bodega_ble

#endif
