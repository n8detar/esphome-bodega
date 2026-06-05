# Bodega BLE ESPHome External Component

Unofficial ESPHome external component for Bodega and Alpicool-style 12V fridge/freezers that use the BLE protocol found in `com.alpicoolneutral.fridge.controller.apk`.

The implementation was built from the APK's JavaScript/Hermes bundle and currently supports:

- BLE service `0x1234`
- Write characteristic `0x1235`
- Notify characteristic `0x1236`
- Querying live state
- Setting left/right/hot targets
- Power on/off
- Control lock
- Fridge panel unit switching (Celsius/Fahrenheit)
- Optional bind command

The component keeps a fixed `report_unit` for ESPHome entities, even if the fridge panel unit changes. That avoids Home Assistant entities bouncing between `degC` and `degF`.

## Repository Layout

Upload this folder as the root of your GitHub repository:

```text
.
├── README.md
├── example.yaml
└── components/
    └── bodega_ble/
        ├── __init__.py
        ├── const.py
        ├── bodega_ble.h
        ├── bodega_ble.cpp
        ├── bodega_ble_number.h
        ├── bodega_ble_number.cpp
        ├── bodega_ble_switch.h
        ├── bodega_ble_switch.cpp
        ├── bodega_ble_select.h
        ├── bodega_ble_select.cpp
        ├── bodega_ble_button.h
        ├── bodega_ble_button.cpp
        ├── sensor.py
        ├── number.py
        ├── switch.py
        ├── select.py
        └── button.py
```

## Local Testing

The included [example.yaml](example.yaml) uses the component locally:

```yaml
external_components:
  - source:
      type: local
      path: /config/esphome/esphome-bodega/components
    components: [bodega_ble]
```

Compile it from this repository root with:

```bash
esphome config example.yaml
esphome compile example.yaml
```

## GitHub Usage

After uploading this folder to a GitHub repository, switch the `external_components` source to your repo:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/n8detar/esphome-bodega
      ref: main
      path: components
    components: [bodega_ble]
```

## Home Assistant Device Grouping

The example uses ESPHome sub-devices so the fridge/freezer appears as a separate Home Assistant device from the ESP controller. Define the fridge under `esphome.devices`, then set `device_id` on each Bodega-related entity. Remove the `device_id` entries if you prefer all entities to stay under the ESP node.

```yaml
esphome:
  name: bodega-fridge-bridge
  devices:
    - id: bodega_fridge_device
      name: Bodega Fridge

sensor:
  - platform: bodega_ble
    bodega_ble_id: fridge_hub
    type: left_current
    name: Left Current Temperature
    device_id: bodega_fridge_device
```

## Component Configuration

```yaml
bodega_ble:
  id: fridge_hub
  ble_client_id: fridge_ble
  report_unit: celsius
  bind_on_connect: false
  update_interval: 30s
```

Options:

- `ble_client_id`: Required ESPHome `ble_client` to use for the fridge.
- `report_unit`: Fixed reporting unit for ESPHome entities. One of `celsius` or `fahrenheit`.
- `bind_on_connect`: Sends the app's bind command immediately after notification setup.
- `update_interval`: Poll interval for query refreshes.

## BLE MAC Discovery

The component also listens to BLE advertisements and logs likely Bodega/Alpicool matches once per boot. Candidates are matched by advertised service UUID `0x1234` or by app-derived advertised-name prefixes such as `MF-`, `A1-`, `WT-0001`, `AK1-`, `AK2-`, `AK3-`, `KGS-`, `KGD-`, `W-`, and `WH-`.

If you do not know the MAC address yet, use a temporary placeholder such as `AA:BB:CC:DD:EE:FF`, install the example, watch ESPHome logs for `Discovered possible Bodega/Alpicool BLE fridge ...`, then replace the placeholder with the logged MAC address.

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

## Notes and Limitations

- `right_target`, `right_current`, and some diagnostic fields are only published when the fridge reports dual-zone data.
- `hot_target` is only published when the fridge reports that field.
- `power`, `lock`, and `unit` writes use the APK's full `set others` command, so the component waits for a valid state first.
- The BLE frame parser handles fragmented notifications and validates the app checksum format.
- BLE access is exclusive. While ESPHome is connected, the Android app generally cannot connect to the same fridge.
