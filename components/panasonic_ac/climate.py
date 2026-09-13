from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_WATT,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, climate, sensor, select, switch, binary_sensor, text_sensor

AUTO_LOAD = ["switch", "sensor", "select", "binary_sensor", "text_sensor"]
DEPENDENCIES = ["uart"]

panasonic_ac_ns = cg.esphome_ns.namespace("panasonic_ac")
PanasonicAC = panasonic_ac_ns.class_(
    "PanasonicAC", cg.Component, uart.UARTDevice, climate.Climate
)
panasonic_ac_cnt_ns = panasonic_ac_ns.namespace("CNT")
PanasonicACCNT = panasonic_ac_cnt_ns.class_("PanasonicACCNT", PanasonicAC)
PanasonicACCZ25 = panasonic_ac_cnt_ns.class_("PanasonicACCZ25", PanasonicACCNT)
panasonic_ac_wlan_ns = panasonic_ac_ns.namespace("WLAN")
PanasonicACWLAN = panasonic_ac_wlan_ns.class_("PanasonicACWLAN", PanasonicAC)

PanasonicACSwitch = panasonic_ac_ns.class_(
    "PanasonicACSwitch", switch.Switch, cg.Component
)
PanasonicACSelect = panasonic_ac_ns.class_(
    "PanasonicACSelect", select.Select, cg.Component
)


CONF_HORIZONTAL_SWING_SELECT = "horizontal_swing_select"
CONF_VERTICAL_SWING_SELECT = "vertical_swing_select"
CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
CONF_OUTSIDE_TEMPERATURE_OFFSET = "outside_temperature_offset"
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_CURRENT_TEMPERATURE_OFFSET = "current_temperature_offset"
CONF_NANOEX_SWITCH = "nanoex_switch"
CONF_ECO_SWITCH = "eco_switch"
CONF_ECONAVI_SWITCH = "econavi_switch"
CONF_MILD_DRY_SWITCH = "mild_dry_switch"
CONF_CURRENT_POWER_CONSUMPTION = "current_power_consumption"
CONF_DEFROST_SENSOR = "defrost_sensor"
CONF_OPERATIONAL_STATE = "operational_state"
CONF_OPERATIONAL_STATE_RAW = "operational_state_raw"
CONF_SELECTED_MODE_RAW = "selected_mode_raw"
CONF_STATUS_MULTIPLEX = "status_multiplex"
CONF_COMPRESSOR_RUNNING = "compressor_running"
CONF_INTAKE_TEMPERATURE = "intake_temperature"
CONF_TEMPERATURE_B21 = "temperature_b21"
CONF_CONTROL_REFERENCE = "control_reference"
CONF_OUTDOOR_POWER_RAW = "outdoor_power_raw"
CONF_OUTDOOR_CURRENT = "outdoor_current"
CONF_WLAN = "wlan"
CONF_CNT = "cnt"

HORIZONTAL_SWING_OPTIONS = ["auto", "left", "left_center", "center", "right_center", "right"]

VERTICAL_SWING_OPTIONS = ["swing", "auto", "up", "up_center", "center", "down_center", "down"]

SWITCH_SCHEMA = switch.switch_schema(PanasonicACSwitch).extend(cv.COMPONENT_SCHEMA)

SELECT_SCHEMA = select.select_schema(PanasonicACSelect)

PANASONIC_COMMON_SCHEMA = {
    cv.Optional(CONF_HORIZONTAL_SWING_SELECT): SELECT_SCHEMA,
    cv.Optional(CONF_VERTICAL_SWING_SELECT): SELECT_SCHEMA,
    cv.Optional(CONF_OUTSIDE_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_DEFROST_SENSOR): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_NANOEX_SWITCH): SWITCH_SCHEMA,
    cv.Optional(CONF_OUTSIDE_TEMPERATURE_OFFSET): cv.int_range(min=-15, max=15),
    cv.Optional(CONF_CURRENT_TEMPERATURE_OFFSET): cv.int_range(min=-15, max=15),
}

PANASONIC_CNT_SCHEMA = {
    cv.Optional(CONF_ECO_SWITCH): SWITCH_SCHEMA,
    cv.Optional(CONF_ECONAVI_SWITCH): SWITCH_SCHEMA,
    cv.Optional(CONF_MILD_DRY_SWITCH): SWITCH_SCHEMA,
    cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_CURRENT_POWER_CONSUMPTION): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_OPERATIONAL_STATE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_OPERATIONAL_STATE_RAW): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_SELECTED_MODE_RAW): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_STATUS_MULTIPLEX): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_COMPRESSOR_RUNNING): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_INTAKE_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_TEMPERATURE_B21): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_CONTROL_REFERENCE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_OUTDOOR_POWER_RAW): sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_OUTDOOR_CURRENT): sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_WLAN: climate.climate_schema(PanasonicACWLAN).extend(PANASONIC_COMMON_SCHEMA).extend(uart.UART_DEVICE_SCHEMA),
        CONF_CNT: climate.climate_schema(PanasonicACCZ25).extend(PANASONIC_COMMON_SCHEMA).extend(PANASONIC_CNT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA),
    }
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_HORIZONTAL_SWING_SELECT in config:
        conf = config[CONF_HORIZONTAL_SWING_SELECT]
        swing_select = await select.new_select(conf, options=HORIZONTAL_SWING_OPTIONS)
        await cg.register_component(swing_select, conf)
        cg.add(var.set_horizontal_swing_select(swing_select))

    if CONF_VERTICAL_SWING_SELECT in config:
        conf = config[CONF_VERTICAL_SWING_SELECT]
        swing_select = await select.new_select(conf, options=VERTICAL_SWING_OPTIONS)
        await cg.register_component(swing_select, conf)
        cg.add(var.set_vertical_swing_select(swing_select))

    if CONF_OUTSIDE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTSIDE_TEMPERATURE])
        cg.add(var.set_outside_temperature_sensor(sens))

    if CONF_DEFROST_SENSOR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DEFROST_SENSOR])
        cg.add(var.set_defrost_sensor(sens))

    if CONF_OUTSIDE_TEMPERATURE_OFFSET in config:
        cg.add(var.set_outside_temperature_offset(config[CONF_OUTSIDE_TEMPERATURE_OFFSET]))

    for s in [CONF_ECO_SWITCH, CONF_NANOEX_SWITCH, CONF_MILD_DRY_SWITCH, CONF_ECONAVI_SWITCH]:
        if s in config:
            conf = config[s]
            a_switch = await switch.new_switch(conf)
            await cg.register_component(a_switch, conf)
            cg.add(getattr(var, f"set_{s}")(a_switch))

    if CONF_CURRENT_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_SENSOR])
        cg.add(var.set_current_temperature_sensor(sens))

    if CONF_CURRENT_TEMPERATURE_OFFSET in config:
        cg.add(var.set_current_temperature_offset(config[CONF_CURRENT_TEMPERATURE_OFFSET]))

    if CONF_CURRENT_POWER_CONSUMPTION in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_POWER_CONSUMPTION])
        cg.add(var.set_current_power_consumption_sensor(sens))

    if CONF_OPERATIONAL_STATE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_OPERATIONAL_STATE])
        cg.add(var.set_operational_state_sensor(sens))

    if CONF_OPERATIONAL_STATE_RAW in config:
        sens = await text_sensor.new_text_sensor(config[CONF_OPERATIONAL_STATE_RAW])
        cg.add(var.set_operational_state_raw_sensor(sens))

    if CONF_SELECTED_MODE_RAW in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SELECTED_MODE_RAW])
        cg.add(var.set_selected_mode_raw_sensor(sens))

    if CONF_STATUS_MULTIPLEX in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATUS_MULTIPLEX])
        cg.add(var.set_status_multiplex_sensor(sens))

    if CONF_COMPRESSOR_RUNNING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_COMPRESSOR_RUNNING])
        cg.add(var.set_compressor_running_sensor(sens))

    if CONF_INTAKE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_INTAKE_TEMPERATURE])
        cg.add(var.set_intake_temperature_sensor(sens))

    if CONF_TEMPERATURE_B21 in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE_B21])
        cg.add(var.set_temperature_b21_sensor(sens))

    if CONF_CONTROL_REFERENCE in config:
        sens = await sensor.new_sensor(config[CONF_CONTROL_REFERENCE])
        cg.add(var.set_control_reference_sensor(sens))

    if CONF_OUTDOOR_POWER_RAW in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_POWER_RAW])
        cg.add(var.set_outdoor_power_raw_sensor(sens))

    if CONF_OUTDOOR_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_CURRENT])
        cg.add(var.set_outdoor_current_sensor(sens))
