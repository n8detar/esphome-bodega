import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.const import CONF_ID

from .const import CONF_BIND_ON_CONNECT, CONF_REPORT_UNIT

DEPENDENCIES = ["ble_client"]

bodega_ble_ns = cg.esphome_ns.namespace("bodega_ble")
TemperatureUnit = bodega_ble_ns.enum("TemperatureUnit")

REPORT_UNITS = {
    "celsius": TemperatureUnit.CELSIUS,
    "fahrenheit": TemperatureUnit.FAHRENHEIT,
}

REPORT_UNIT_BY_ID = {}


def get_report_unit_for_id(bodega_id):
    if bodega_id.id in REPORT_UNIT_BY_ID:
        return REPORT_UNIT_BY_ID[bodega_id.id]

    configs = CORE.config.get("bodega_ble", [])
    if isinstance(configs, dict):
        configs = [configs]

    for config in configs:
        if config[CONF_ID].id == bodega_id.id:
            return config.get(CONF_REPORT_UNIT, "celsius")

    return "celsius"


BodegaBLE = bodega_ble_ns.class_(
    "BodegaBLE",
    ble_client.BLEClientNode,
    cg.PollingComponent,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BodegaBLE),
            cv.Optional(CONF_REPORT_UNIT, default="celsius"): cv.one_of(
                *REPORT_UNITS, lower=True
            ),
            cv.Optional(CONF_BIND_ON_CONNECT, default=False): cv.boolean,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    REPORT_UNIT_BY_ID[config[CONF_ID].id] = config[CONF_REPORT_UNIT]
    cg.add(var.set_report_unit(REPORT_UNITS[config[CONF_REPORT_UNIT]]))
    cg.add(var.set_bind_on_connect(config[CONF_BIND_ON_CONNECT]))
