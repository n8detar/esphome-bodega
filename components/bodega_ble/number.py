import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
)

from . import BodegaBLE, bodega_ble_ns, get_report_unit_for_id
from .const import (
    CONF_BODEGA_BLE_ID,
    TYPE_HOT_TARGET,
    TYPE_LEFT_TARGET,
    TYPE_RIGHT_TARGET,
)

DEPENDENCIES = ["bodega_ble"]

BodegaBLENumber = bodega_ble_ns.class_(
    "BodegaBLENumber", number.Number, cg.Parented.template(BodegaBLE)
)
NumberKind = bodega_ble_ns.enum("NumberKind")

NUMBER_TYPES = {
    TYPE_LEFT_TARGET: NumberKind.LEFT_TARGET,
    TYPE_RIGHT_TARGET: NumberKind.RIGHT_TARGET,
    TYPE_HOT_TARGET: NumberKind.HOT_TARGET,
}

CONFIG_SCHEMA = number.number_schema(
    BodegaBLENumber,
    device_class=DEVICE_CLASS_TEMPERATURE,
).extend(
    {
        cv.GenerateID(CONF_BODEGA_BLE_ID): cv.use_id(BodegaBLE),
        cv.Required(CONF_TYPE): cv.enum(NUMBER_TYPES, lower=True),
    }
)


UNIT_FAHRENHEIT = "\u00b0F"


def temperature_unit_for_config(config):
    report_unit = get_report_unit_for_id(config[CONF_BODEGA_BLE_ID])
    return UNIT_FAHRENHEIT if report_unit == "fahrenheit" else UNIT_CELSIUS


async def to_code(config):
    if CONF_UNIT_OF_MEASUREMENT not in config:
        config[CONF_UNIT_OF_MEASUREMENT] = temperature_unit_for_config(config)

    var = await number.new_number(
        config,
        NUMBER_TYPES[config[CONF_TYPE]],
        min_value=-40,
        max_value=140,
        step=1,
    )
    parent = await cg.get_variable(config[CONF_BODEGA_BLE_ID])
    await cg.register_parented(var, parent)

    setter = {
        TYPE_LEFT_TARGET: "set_left_target_number",
        TYPE_RIGHT_TARGET: "set_right_target_number",
        TYPE_HOT_TARGET: "set_hot_target_number",
    }[config[CONF_TYPE]]
    cg.add(getattr(parent, setter)(var))
