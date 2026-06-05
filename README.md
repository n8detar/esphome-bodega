# ESPHome Bodega Fridge/Freezer

ESPHome external component for BLE-controlled Bodega and Alpicool-style 12 V fridge/freezers that use the protocol found in `com.alpicoolneutral.fridge.controller.apk`.

> Disclaimer: This project is vibe-coded and provided as-is. It controls real compressor hardware over BLE, including power state and temperature setpoints, so review the code and configuration, test carefully, and use at your own risk.

This repository contains one component:

- `bodega_ble`: a BLE fridge/freezer integration that connects through ESPHome `ble_client`, reads the fridge state, exposes Home Assistant entities, and sends the APK-derived control commands back to the unit.

The implementation was built from the APK's JavaScript/Hermes bundle. The protocol uses BLE service `0x1234`, write characteristic `0x1235`, notify characteristic `0x1236`, framed packets beginning with `FE FE`, and the app's checksum format.

## Features

- Live fridge/freezer state polling.
- Left-zone and right-zone current temperature sensors.
- Left-zone, right-zone, and hot-box target temperature numbers when reported by the fridge.
- Battery percentage and battery voltage sensors.
- Diagnostic sensors for model, run mode, battery saver, running status, warning code, sleep mode, partition mode, and disinfect mode.
- Power and control-lock switches.
- Fridge panel unit selector for Celsius/Fahrenheit.
- Refresh and optional bind buttons.
- Fixed ESPHome `report_unit` for Home Assistant entities, with internal conversion when the fridge panel unit changes.
- BLE MAC discovery logging for likely Bodega/Alpicool advertisements.
- ESPHome sub-device support through standard `device_id` on every entity.
- Fragmented notification handling and checksum validation.

## Examples

### Minimal Fridge Bridge

```yaml
external_components:
  - source: github://n8detar/esphome-bodega@main
    components:
      - bodega_ble

esphome:
  name: bodega-fridge-bridge
  friendly_name: Bodega Fridge Bridge

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:
api:
ota:
  platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

esp32_ble_tracker:

ble_client:
  - mac_address: AA:BB:CC:DD:EE:FF
    id: fridge_ble
    auto_connect: true

bodega_ble:
  id: fridge_hub
  ble_client_id: fridge_ble
  report_unit: celsius

sensor:
  - platform: bodega_ble
    bodega_ble_id: fridge_hub
    type: left_current
    name: Left Current Temperature

number:
  - platform: bodega_ble
    bodega_ble_id: fridge_hub
    type: left_target
    name: Left Target Temperature

switch:
  - platform: bodega_ble
    bodega_ble_id: fridge_hub
    type: power
    name: Fridge Power
```

### Fridge Sub-Device

This groups the fridge/freezer as a separate Home Assistant device from the ESP controller. Define the fridge under `esphome.devices`, then set `device_id` on each Bodega-related entity.

```yaml
esphome:
  name: bodega-fridge-bridge
  friendly_name: Bodega Fridge Bridge
  devices:
    - id: bodega_fridge_device
      name: Bodega Fridge

sensor:
  - platform: bodega_ble
    bodega_ble_id: fridge_hub
    type: left_current
    name: Left Current Temperature
    device_id: bodega_fridge_device

number:
  - platform: bodega_ble
    bodega_ble_id: fridge_hub
    type: left_target
    name: Left Target Temperature
    device_id: bodega_fridge_device

switch:
  - platform: ble_client
    ble_client_id: fridge_ble
    name: Fridge BLE Connection
    device_id: bodega_fridge_device
```

See [example.yaml](example.yaml) for the full default example.

### Fahrenheit Reporting

The fridge can be set to Celsius or Fahrenheit on its own panel. `report_unit` controls the unit exposed to ESPHome and Home Assistant, and the component converts values internally when needed.

```yaml
bodega_ble:
  id: fridge_hub
  ble_client_id: fridge_ble
  report_unit: fahrenheit
  update_interval: 30s
```

## BLE MAC Discovery

If you do not know the fridge MAC address yet, flash with a temporary placeholder such as `AA:BB:CC:DD:EE:FF` and watch ESPHome logs.

The component listens to BLE advertisements and logs likely matches once per boot:

```text
Discovered possible Bodega/Alpicool BLE fridge AA:BB:CC:DD:EE:FF RSSI=-62 ...
```

Candidates are matched by advertised service UUID `0x1234` or by app-derived advertised-name prefixes such as `MF-`, `A1-`, `WT-0001`, `AK1-`, `AK2-`, `AK3-`, `KGS-`, `KGD-`, `W-`, and `WH-`.

Replace the placeholder `ble_client.mac_address` with the logged MAC address after you identify the fridge.

## Configuration

```yaml
bodega_ble:
  id: fridge_hub
  ble_client_id: fridge_ble
  report_unit: celsius
  bind_on_connect: false
  update_interval: 30s
```

- `ble_client_id`: required ESPHome `ble_client` to use for the fridge.
- `report_unit`: fixed reporting unit for ESPHome entities. Valid values are `celsius` and `fahrenheit`.
- `bind_on_connect`: sends the APK's bind command immediately after notification setup.
- `update_interval`: poll interval for query refreshes.

## Supported Entities

Sensors:

- `left_current`
- `right_current`
- `battery_percentage`
- `battery_voltage`
- `model`
- `run_mode`
- `battery_saver`
- `running_status`
- `warning_code`
- `sleep_mode`
- `partition_mode`
- `disinfect_mode`

Numbers:

- `left_target`
- `right_target`
- `hot_target`

Switches:

- `power`
- `lock`

Selects:

- `unit`

Buttons:

- `refresh`
- `bind`

## Temperature Units

The fridge panel unit and Home Assistant entity unit are intentionally separate. The `unit` select changes the fridge's own display setting. The `report_unit` option controls how ESPHome publishes current and target temperatures.

Keeping `report_unit` fixed avoids Home Assistant entities bouncing between `°C` and `°F` if the fridge panel unit is changed from the app, the front panel, or ESPHome.

## BLE Behavior

The component connects through ESPHome `ble_client`, discovers the write and notify characteristics, subscribes to notifications, and periodically sends the app's query command. Writes are queued and sent only after the BLE connection and GATT handles are ready.

BLE access is generally exclusive. While ESPHome is connected, the Android app usually cannot connect to the same fridge.

## Notes and Limitations

- `right_target`, `right_current`, and some diagnostic fields are only published when the fridge reports dual-zone data.
- `hot_target` is only published when the fridge reports that field.
- `power`, `lock`, and `unit` writes use the APK's full `set others` command, so the component waits for a valid state first.
- The bind command is included because it exists in the APK protocol, but not every fridge requires it.
- The protocol was reverse engineered from the Android APK and has not been validated against every Bodega, Alpicool, or rebranded model.

## Credits

- Protocol reference from `com.alpicoolneutral.fridge.controller.apk`.
- Built for ESPHome using the standard `ble_client` and `esp32_ble_tracker` components.

## License

MIT. See [LICENSE](LICENSE).
