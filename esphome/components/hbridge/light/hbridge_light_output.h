#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

// Using PollingComponent as the updates are more consistent and reduces flickering
class HBridgeLightOutput : public Component, public light::LightOutput {
 public:
  HBridgeLightOutput() : Component() {}

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
    xTaskCreate(this->updateTask, "hbridge_update", 8*1024, (void*)this, 1, NULL);
  }

  static void updateTask(void * taskParam) {
    HBridgeLightOutput * hbridgeOutput = (HBridgeLightOutput *)taskParam;
    bool lightA = false;
    const TickType_t interval_on = pdMS_TO_TICKS(10);
    const TickType_t interval_off = pdMS_TO_TICKS(100);
    TickType_t interval;
    TickType_t now = xTaskGetTickCount();
    while (1)
    {
        if (hbridgeOutput->pina_duty_ == 0 && hbridgeOutput->pinb_duty_ == 0) {
          interval = interval_off;
        } else {
          interval = interval_on;
        }

        if(lightA){
          hbridgeOutput->pinb_pin_->set_level(0);
          hbridgeOutput->pina_pin_->set_level(hbridgeOutput->pina_duty_);
        } else {
          hbridgeOutput->pina_pin_->set_level(0);
          hbridgeOutput->pinb_pin_->set_level(hbridgeOutput->pinb_duty_);
        }
        lightA = !lightA;

        vTaskDelayUntil(&now, interval);
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
