import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from . import BodegaBLE, bodega_ble_ns
from .const import CONF_BODEGA_BLE_ID, TYPE_UNIT

DEPENDENCIES = ["bodega_ble"]

BodegaBLESelect = bodega_ble_ns.class_(
    "BodegaBLESelect", select.Select, cg.Parented.template(BodegaBLE)
)
SelectKind = bodega_ble_ns.enum("SelectKind")

SELECT_TYPES = {
    TYPE_UNIT: SelectKind.UNIT,
}

OPTIONS = ["celsius", "fahrenheit"]

CONFIG_SCHEMA = select.select_schema(BodegaBLESelect).extend(
    {
        cv.GenerateID(CONF_BODEGA_BLE_ID): cv.use_id(BodegaBLE),
        cv.Required(CONF_TYPE): cv.enum(SELECT_TYPES, lower=True),
    }
)


async def to_code(config):
    var = await select.new_select(
        config,
        SELECT_TYPES[config[CONF_TYPE]],
        options=OPTIONS,
    )
    parent = await cg.get_variable(config[CONF_BODEGA_BLE_ID])
    await cg.register_parented(var, parent)
    cg.add(parent.set_unit_select(var))
