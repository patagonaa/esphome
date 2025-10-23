#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace brose_vm_iic {

class BroseVmIicComponent;

using brose_vm_iic_writer_t = std::function<void(BroseVmIicComponent &)>;

class BroseVmIicComponent : public display::DisplayBuffer {
 public:
  void set_writer(brose_vm_iic_writer_t &&writer);

  void setup() override;

  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;

  void display();

  void set_i2c_bus(i2c::I2CBus *bus) { bus_ = bus; }
  void set_module_mapping(uint8_t i, uint8_t v) { this->moduleMapping_[i] = v; }
  void set_size(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  void set_flip_time(uint16_t f) { this->flipTime_ = f; }

  int get_height_internal() override { return this->height_; }
  int get_width_internal() override { return this->width_; };

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

 protected:
  uint8_t reverse_(uint8_t b);
  void generateDataPacket_(uint8_t moduleSelect, uint8_t colAddr, bool colData, uint8_t rowAddr, bool rowLData,
                           bool rowLEnable, bool rowHData, bool rowHEnable);
  void writeDot_(uint8_t x, uint8_t y, bool state);
  bool getDotFromBuffer_(uint8_t x, uint8_t y, uint8_t *buf);
  bool dotChanged_(uint8_t x, uint8_t y);
  bool getDot_(uint8_t x, uint8_t y);

  void i2cWriteByte_(uint8_t addr, uint8_t val);

  i2c::I2CBus *bus_{nullptr};

  uint16_t width_;
  uint16_t height_;
  uint8_t moduleMapping_[8]{0};
  uint16_t flipTime_;
  uint8_t i2cBuf_[3] = {0};

  uint16_t frameBufferWidth_;
  uint16_t frameBufferSize_;

  uint8_t prevColumnState_{0xFF};
  uint8_t prevModuleState_{0};

  optional<brose_vm_iic_writer_t> writer_{};
};

}  // namespace brose_vm_iic
}  // namespace esphome
