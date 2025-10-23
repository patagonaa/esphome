import esphome.codegen as cg
from esphome.components import display
from esphome.components.i2c import I2CBus
import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_I2C_ID, CONF_ID, CONF_LAMBDA, CONF_WIDTH

DEPENDENCIES = ["i2c"]

brose_vm_iic_ns = cg.esphome_ns.namespace("brose_vm_iic")
BroseVmIicComponent = brose_vm_iic_ns.class_(
    "BroseVmIicComponent", cg.PollingComponent, display.DisplayBuffer
)
BroseVmIicComponentRef = BroseVmIicComponent.operator("ref")

CONF_MODULE_MAPPING = "module_mapping"
CONF_FLIP_TIME = "flip_time"

CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BroseVmIicComponent),
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CBus),
        cv.Required(CONF_MODULE_MAPPING): cv.ensure_list(cv.uint8_t),
        cv.Required(CONF_WIDTH): cv.uint16_t,
        cv.Required(CONF_HEIGHT): cv.uint16_t,
        cv.Optional(CONF_FLIP_TIME, default=550): cv.uint16_t,
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    parent = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(parent))

    for idx, x in enumerate(config[CONF_MODULE_MAPPING]):
        if x < 8:
            cg.add(var.set_module_mapping(idx, x))

    cg.add(var.set_size(config[CONF_WIDTH], config[CONF_HEIGHT]))
    cg.add(var.set_flip_time(config[CONF_FLIP_TIME]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(BroseVmIicComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
