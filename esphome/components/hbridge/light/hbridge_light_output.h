#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

// Using PollingComponent as the updates are more consistent and reduces flickering
class HBridgeLightOutput : public PollingComponent, public light::LightOutput {
 public:
  HBridgeLightOutput() : PollingComponent(1) {}

  void set_pina_pin(output::FloatOutput *pina_pin) { pina_pin_ = pina_pin; }
  void set_pinb_pin(output::FloatOutput *pinb_pin) { pinb_pin_ = pinb_pin; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(153);
    traits.set_max_mireds(500);
    return traits;
  }

  void setup() override {
    xTaskCreate(this->updateTask, "hbridge_update", 8*1024, this, 2, NULL);
  }

  void update() override {
    // This method runs around 60 times per second
    // We cannot do the PWM ourselves so we are reliant on the hardware PWM

  }

  static void updateTask(void * taskParam) {
    HBridgeLightOutput * hbridgeOutput = (HBridgeLightOutput *)taskParam;
    int i = 0;
    while (1)
    {
        if (hbridgeOutput->pina_duty_ == 0 && hbridgeOutput->pinb_duty_ == 0){
          delay(100);
        }

        hbridgeOutput->pinb_pin_->set_level(0);
        hbridgeOutput->pina_pin_->set_level(hbridgeOutput->pina_duty_);
        delay(10);
        hbridgeOutput->pina_pin_->set_level(0);
        hbridgeOutput->pinb_pin_->set_level(hbridgeOutput->pinb_duty_);
        delay(10);
    }
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void write_state(light::LightState *state) override {
    state->current_values_as_cwww(&this->pina_duty_, &this->pinb_duty_, false);
  }

 protected:
  output::FloatOutput *pina_pin_;
  output::FloatOutput *pinb_pin_;
  float pina_duty_ = 0;
  float pinb_duty_ = 0;
};

}  // namespace hbridge
}  // namespace esphome
