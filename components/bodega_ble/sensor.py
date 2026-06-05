import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from . import BodegaBLE, get_report_unit_for_id
from .const import (
    CONF_BODEGA_BLE_ID,
    CONF_TYPE,
    TYPE_BATTERY_PERCENTAGE,
    TYPE_BATTERY_SAVER,
    TYPE_BATTERY_VOLTAGE,
    TYPE_DISINFECT_MODE,
    TYPE_LEFT_CURRENT,
    TYPE_MODEL,
    TYPE_PARTITION_MODE,
    TYPE_RIGHT_CURRENT,
    TYPE_RUN_MODE,
    TYPE_RUNNING_STATUS,
    TYPE_SLEEP_MODE,
    TYPE_WARNING_CODE,
)

DEPENDENCIES = ["bodega_ble"]

SENSOR_TYPES = {
    TYPE_LEFT_CURRENT: "set_left_current_sensor",
    TYPE_RIGHT_CURRENT: "set_right_current_sensor",
    TYPE_BATTERY_PERCENTAGE: "set_battery_percentage_sensor",
    TYPE_BATTERY_VOLTAGE: "set_battery_voltage_sensor",
    TYPE_MODEL: "set_model_sensor",
    TYPE_RUN_MODE: "set_run_mode_sensor",
    TYPE_BATTERY_SAVER: "set_battery_saver_sensor",
    TYPE_RUNNING_STATUS: "set_running_status_sensor",
    TYPE_WARNING_CODE: "set_warning_code_sensor",
    TYPE_SLEEP_MODE: "set_sleep_mode_sensor",
    TYPE_PARTITION_MODE: "set_partition_mode_sensor",
    TYPE_DISINFECT_MODE: "set_disinfect_mode_sensor",
}

CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_BODEGA_BLE_ID): cv.use_id(BodegaBLE),
        cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
    }
)


UNIT_FAHRENHEIT = "\u00b0F"


def temperature_unit_for_config(config):
    report_unit = get_report_unit_for_id(config[CONF_BODEGA_BLE_ID])
    return UNIT_FAHRENHEIT if report_unit == "fahrenheit" else UNIT_CELSIUS


def apply_default(config, key, value):
    if key not in config:
        config[key] = value


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    needs_measurement_state_class = sensor_type in (
        TYPE_LEFT_CURRENT,
        TYPE_RIGHT_CURRENT,
        TYPE_BATTERY_PERCENTAGE,
        TYPE_BATTERY_VOLTAGE,
    )

    if sensor_type in (TYPE_LEFT_CURRENT, TYPE_RIGHT_CURRENT):
        apply_default(config, CONF_DEVICE_CLASS, DEVICE_CLASS_TEMPERATURE)
        apply_default(config, CONF_UNIT_OF_MEASUREMENT, temperature_unit_for_config(config))
        apply_default(config, CONF_ACCURACY_DECIMALS, 0)
    elif sensor_type == TYPE_BATTERY_PERCENTAGE:
        apply_default(config, CONF_DEVICE_CLASS, DEVICE_CLASS_BATTERY)
        apply_default(config, CONF_UNIT_OF_MEASUREMENT, UNIT_PERCENT)
        apply_default(config, CONF_ACCURACY_DECIMALS, 0)
    elif sensor_type == TYPE_BATTERY_VOLTAGE:
        apply_default(config, CONF_DEVICE_CLASS, DEVICE_CLASS_VOLTAGE)
        apply_default(config, CONF_UNIT_OF_MEASUREMENT, UNIT_VOLT)
        apply_default(config, CONF_ACCURACY_DECIMALS, 1)
    else:
        apply_default(config, CONF_ACCURACY_DECIMALS, 0)

    var = await sensor.new_sensor(config)
    if needs_measurement_state_class and CONF_STATE_CLASS not in config:
        cg.add(var.set_state_class(sensor.STATE_CLASSES[STATE_CLASS_MEASUREMENT]))
    parent = await cg.get_variable(config[CONF_BODEGA_BLE_ID])
    cg.add(getattr(parent, SENSOR_TYPES[config[CONF_TYPE]])(var))
