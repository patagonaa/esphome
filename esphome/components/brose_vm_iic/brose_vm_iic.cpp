#include "brose_vm_iic.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace brose_vm_iic {

static const char *const TAG = "brose_vm_iic";

float BroseVmIicComponent::get_setup_priority() const { return setup_priority::PROCESSOR; }
void BroseVmIicComponent::setup() {
  this->frameBufferWidth_ = (this->width_ + 7) / 8;
  this->frameBufferSize_ = this->frameBufferWidth_ * this->height_;

  // double buffer (current buffer, last buffer)
  this->init_internal_(this->frameBufferSize_ * 2);

  if (this->buffer_ == nullptr) {
    this->mark_failed();
    return;
  }

  memset(this->buffer_, 0, this->frameBufferSize_ * 2);
}
void BroseVmIicComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Brose VM-IIC:\n");
  LOG_UPDATE_INTERVAL(this);
}

void HOT BroseVmIicComponent::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (this->buffer_ == nullptr)
    return;
  if (color.is_on()) {
    this->buffer_[y * this->frameBufferWidth_ + x / 8] |= 0x80 >> (x % 8);
  } else {
    this->buffer_[y * this->frameBufferWidth_ + x / 8] &= ~(0x80 >> (x % 8));
  }
}

bool BroseVmIicComponent::getDotFromBuffer_(uint8_t x, uint8_t y, uint8_t *buf) {
  return (buf[y * this->frameBufferWidth_ + x / 8] & (0x80 >> (x % 8))) > 0;
}

bool BroseVmIicComponent::dotChanged_(uint8_t x, uint8_t y) {
  return this->getDotFromBuffer_(x, y, this->buffer_) ^
         this->getDotFromBuffer_(x, y, this->buffer_ + this->frameBufferSize_);
}

bool BroseVmIicComponent::getDot_(uint8_t x, uint8_t y) { return this->getDotFromBuffer_(x, y, this->buffer_); }

uint8_t BroseVmIicComponent::reverse_(uint8_t b) {  // converts MSB first to LSB first (and vice versa)
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void BroseVmIicComponent::generateDataPacket_(uint8_t moduleSelect, uint8_t colAddr, bool colData, uint8_t rowAddr,
                                              bool rowLData, bool rowLEnable, bool rowHData, bool rowHEnable) {
  // display expects it in the order A0 A1 A2 B0 B1 (LSB first)
  colAddr = (colAddr & 0x01) << 4 | (colAddr & 0x02) << 2 | (colAddr & 0x04) << 0 | (colAddr & 0x08) >> 2 |
            (colAddr & 0x10) >> 4;

  // order: B0 B1 A0 A1 A2
  rowAddr = (rowAddr & 0x01) << 2 | (rowAddr & 0x02) << 0 | (rowAddr & 0x04) >> 2 | (rowAddr & 0x08) << 1 |
            (rowAddr & 0x10) >> 1;

  // module select is LSB fist
  this->i2cBuf_[0] = ~(this->reverse_(moduleSelect));

  this->i2cBuf_[1] = (colAddr % 0x20) << 1;
  this->i2cBuf_[1] |= colData << 6;
  this->i2cBuf_[1] |= (rowAddr & 0x01) << 7;

  this->i2cBuf_[2] = rowAddr >> 1;
  this->i2cBuf_[2] |= rowLData << 4;
  this->i2cBuf_[2] |= rowLEnable << 5;
  this->i2cBuf_[2] |= rowHData << 6;
  this->i2cBuf_[2] |= rowHEnable << 7;
}

void BroseVmIicComponent::writeDot_(uint8_t x, uint8_t y, bool state) {
  // calculate digit (B0/B1) and segment (A0-A2) of adress
  uint8_t colFpDigit = (x % 28) / 7;
  uint8_t colFpSegment = x % 7 + 1;

  uint8_t moduleNum = this->moduleMapping_[(x / 28)] - 1;
  uint8_t moduleBits = 1 << moduleNum;

  uint8_t rowFpDigit = ((y % 14) / 7) * 2 + !state;  // even digits are set, odd digits are unset
  uint8_t rowFpSegment = y % 7 + 1;

  bool rowLowDriver = y < 14;

  uint8_t colAddr = colFpDigit << 3 | colFpSegment;
  uint8_t rowAddr = rowFpDigit << 3 | rowFpSegment;

  this->generateDataPacket_(moduleBits, colAddr, !state, rowAddr, state, rowLowDriver, state, !rowLowDriver);

  // Serial.printf("x=%3d y=%2d state=%d moduleBits=%02X colAddr=%02X rowAddr=%02X buf= %02X %02X %02X\n", x, y, state,
  // moduleBits, colAddr, rowAddr, this->i2cBuf_[0], this->i2cBuf_[1], this->i2cBuf_[2]);

  bool xStateChanged =
      this->i2cBuf_[0] != this->prevModuleState_ || (this->i2cBuf_[1] & 0x7E) != this->prevColumnState_;

  if (xStateChanged) {
    // flip module ENABLE back and forth if state of the columns changed
    this->i2cWriteByte_(0x40, 0xFF);  // disable all modules
  }
  this->i2cWriteByte_(0x42, this->i2cBuf_[1]);
  this->i2cWriteByte_(0x44, this->i2cBuf_[2]);
  if (xStateChanged) {
    this->i2cWriteByte_(0x40, this->i2cBuf_[0]);
    this->prevModuleState_ = this->i2cBuf_[0];
    this->prevColumnState_ = this->i2cBuf_[1] & 0x7E;
  }

  delayMicroseconds(this->flipTime_);

  this->i2cBuf_[2] &= 0x0F;  // only clear row driver enables
  this->i2cWriteByte_(0x44, this->i2cBuf_[2]);
}

void BroseVmIicComponent::i2cWriteByte_(uint8_t addr, uint8_t val) {
  this->bus_->write_readv(addr, &val, 1, nullptr, 0, false);
}

void BroseVmIicComponent::display() {
  this->prevModuleState_ = 0;
  this->prevColumnState_ = 0xFF;
  for (uint8_t x = 0; x < this->width_; x++) {
    for (uint8_t y = 0; y < this->height_; y++) {
      if (this->dotChanged_(x, y)) {
        this->writeDot_(x, y, this->getDot_(x, y));
      }
    }
  }
  this->i2cWriteByte_(0x40, 0xFF);  // disable all modules

  // store previous frame Buffer
  memcpy(this->buffer_ + this->frameBufferSize_, this->buffer_, this->frameBufferSize_);
}
void BroseVmIicComponent::update() {
  this->do_update_();
  this->display();
}
void BroseVmIicComponent::set_writer(brose_vm_iic_writer_t &&writer) { this->writer_ = writer; }

}  // namespace brose_vm_iic
}  // namespace esphome
