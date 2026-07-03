from esphome import core, pins
import esphome.codegen as cg
from esphome.components import display, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

CONF_LUT_MODE = "lut_mode"

uc8179_gray4_ns = cg.esphome_ns.namespace("uc8179_gray4")
UC8179Gray4 = uc8179_gray4_ns.class_(
    "UC8179Gray4", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

LutMode = uc8179_gray4_ns.enum("LutMode")
LUT_MODES = {
    "custom": LutMode.LUT_MODE_CUSTOM,
    "otp": LutMode.LUT_MODE_OTP,
}

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(UC8179Gray4),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_LUT_MODE, default="custom"): cv.enum(
                LUT_MODES, lower=True
            ),
            cv.Optional(CONF_RESET_DURATION): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=500)),
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "uc8179_gray4", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))
    busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy))

    cg.add(var.set_lut_mode(config[CONF_LUT_MODE]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_DURATION in config:
        cg.add(var.set_reset_duration(config[CONF_RESET_DURATION]))
