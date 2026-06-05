import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_DIAGNOSTIC

from . import BodegaBLE, bodega_ble_ns
from .const import CONF_BODEGA_BLE_ID, TYPE_BIND, TYPE_REFRESH

DEPENDENCIES = ["bodega_ble"]

BodegaBLEButton = bodega_ble_ns.class_(
    "BodegaBLEButton", button.Button, cg.Parented.template(BodegaBLE)
)
ButtonKind = bodega_ble_ns.enum("ButtonKind")

BUTTON_TYPES = {
    TYPE_REFRESH: ButtonKind.REFRESH,
    TYPE_BIND: ButtonKind.BIND,
}

CONFIG_SCHEMA = button.button_schema(
    BodegaBLEButton,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_BODEGA_BLE_ID): cv.use_id(BodegaBLE),
        cv.Required(CONF_TYPE): cv.enum(BUTTON_TYPES, lower=True),
    }
)


async def to_code(config):
    var = await button.new_button(config, BUTTON_TYPES[config[CONF_TYPE]])
    parent = await cg.get_variable(config[CONF_BODEGA_BLE_ID])
    await cg.register_parented(var, parent)

    setter = {
        TYPE_REFRESH: "set_refresh_button",
        TYPE_BIND: "set_bind_button",
    }[config[CONF_TYPE]]
    cg.add(getattr(parent, setter)(var))
