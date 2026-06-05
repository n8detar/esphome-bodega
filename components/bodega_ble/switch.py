import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from . import BodegaBLE, bodega_ble_ns
from .const import CONF_BODEGA_BLE_ID, TYPE_LOCK, TYPE_POWER

DEPENDENCIES = ["bodega_ble"]

BodegaBLESwitch = bodega_ble_ns.class_(
    "BodegaBLESwitch", switch.Switch, cg.Parented.template(BodegaBLE)
)
SwitchKind = bodega_ble_ns.enum("SwitchKind")

SWITCH_TYPES = {
    TYPE_POWER: SwitchKind.POWER,
    TYPE_LOCK: SwitchKind.LOCK,
}

CONFIG_SCHEMA = switch.switch_schema(
    BodegaBLESwitch,
    block_inverted=True,
    default_restore_mode="DISABLED",
).extend(
    {
        cv.GenerateID(CONF_BODEGA_BLE_ID): cv.use_id(BodegaBLE),
        cv.Required(CONF_TYPE): cv.enum(SWITCH_TYPES, lower=True),
    }
)


async def to_code(config):
    var = await switch.new_switch(config, SWITCH_TYPES[config[CONF_TYPE]])
    parent = await cg.get_variable(config[CONF_BODEGA_BLE_ID])
    await cg.register_parented(var, parent)

    setter = {
        TYPE_POWER: "set_power_switch",
        TYPE_LOCK: "set_lock_switch",
    }[config[CONF_TYPE]]
    cg.add(getattr(parent, setter)(var))
