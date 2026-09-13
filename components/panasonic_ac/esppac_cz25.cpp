#include "esppac_cnt.h"

#include "esphome/core/log.h"

#include <cstdio>

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const char *const TAG_CZ25 = "panasonic_ac.cz25";

std::string PanasonicACCZ25::determine_operational_state_(uint8_t state) const {
  switch (state) {
    case 0x00:
      if (this->mode == climate::CLIMATE_MODE_OFF)
        return "OFF";
      if (this->mode == climate::CLIMATE_MODE_HEAT_COOL)
        return "AUTO_IDLE";
      return "IDLE_0x00";
    case 0x0C:
      return "AUTO_COOL_RUN";
    case 0x20:
      return "DRY_IDLE";
    case 0x24:
      return "DRY_TRANS";
    case 0x28:
      return "DRY_START";
    case 0x2C:
      if (this->mode == climate::CLIMATE_MODE_HEAT_COOL)
        return "AUTO_HEAT_RUN";
      return "DRY_RUN";
    case 0x30:
      return "COOL_IDLE";
    case 0x34:
      return "COOL_TRANS";
    case 0x38:
      return "COOL_START";
    case 0x3C:
      return "COOL_RUN";
    case 0x40:
      return "HEAT_IDLE";
    case 0x44:
      return "HEAT_TRANS";
    case 0x48:
      return "HEAT_START";
    case 0x4C:
      return "HEAT_RUN";
    case 0x60:
      return "FAN";
    default:
      break;
  }

  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "UNKNOWN_0x%02X", state);
  return std::string(buffer);
}

bool PanasonicACCZ25::determine_compressor_running_(uint8_t state) const {
  switch (state) {
    case 0x0C:  // AUTO cooling side, observed on CZ25
    case 0x28:  // DRY start
    case 0x2C:  // DRY run / AUTO heating side
    case 0x38:  // COOL start
    case 0x3C:  // COOL run
    case 0x48:  // HEAT start
    case 0x4C:  // HEAT run
      return true;
    default:
      return false;
  }
}

climate::ClimateAction PanasonicACCZ25::determine_action_from_cnt_state_(uint8_t state) {
  if (this->mode == climate::CLIMATE_MODE_OFF)
    return climate::CLIMATE_ACTION_OFF;

  if (this->mode == climate::CLIMATE_MODE_FAN_ONLY || state == 0x60)
    return climate::CLIMATE_ACTION_FAN;

  // AUTO has two CZ25-specific active states observed in the captured logs.
  if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
    if (state == 0x00)
      return climate::CLIMATE_ACTION_IDLE;
    if (state == 0x0C)
      return climate::CLIMATE_ACTION_COOLING;
    if (state == 0x2C)
      return climate::CLIMATE_ACTION_HEATING;
  }

  // Decode the physical state family even when a newly selected mode has not
  // yet propagated to the indoor unit state machine.
  switch (state) {
    case 0x20:
      return climate::CLIMATE_ACTION_IDLE;
    case 0x24:
    case 0x28:
    case 0x2C:
      return climate::CLIMATE_ACTION_DRYING;
    case 0x30:
      return climate::CLIMATE_ACTION_IDLE;
    case 0x34:
    case 0x38:
    case 0x3C:
      return climate::CLIMATE_ACTION_COOLING;
    case 0x40:
      return climate::CLIMATE_ACTION_IDLE;
    case 0x44:
    case 0x48:
    case 0x4C:
      return climate::CLIMATE_ACTION_HEATING;
    default:
      break;
  }

  // Unknown/transient state: retain the upstream temperature-based fallback.
  return this->determine_action();
}

void PanasonicACCZ25::publish_cnt_telemetry_() {
  if (this->rx_buffer_.size() < 31)
    return;

  const uint8_t state = this->rx_buffer_[12];

  if (this->operational_state_sensor_ != nullptr)
    this->operational_state_sensor_->publish_state(this->determine_operational_state_(state));

  if (this->operational_state_raw_sensor_ != nullptr) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "0x%02X", state);
    this->operational_state_raw_sensor_->publish_state(buffer);
  }

  if (this->compressor_running_sensor_ != nullptr)
    this->compressor_running_sensor_->publish_state(this->determine_compressor_running_(state));

  // b18 is Panasonic's primary indoor/current-temperature field on this CZ25.
  if (this->intake_temperature_sensor_ != nullptr && this->rx_buffer_[18] != 0x80)
    this->intake_temperature_sensor_->publish_state((int8_t) this->rx_buffer_[18]);

  // Keep b21 deliberately neutral; its physical meaning is still model-dependent.
  if (this->temperature_b21_sensor_ != nullptr && this->rx_buffer_[21] != 0x80)
    this->temperature_b21_sensor_->publish_state((int8_t) this->rx_buffer_[21]);

  // Experimental internal control/reference field, encoded in 0.5 C steps.
  if (this->control_reference_sensor_ != nullptr && this->rx_buffer_[13] != 0x80)
    this->control_reference_sensor_->publish_state(((int8_t) this->rx_buffer_[13]) / 2.0f);

  if (this->outdoor_power_raw_sensor_ != nullptr) {
    const uint16_t raw_power = (uint16_t) this->rx_buffer_[28] + ((uint16_t) this->rx_buffer_[29] << 8);
    this->outdoor_power_raw_sensor_->publish_state(raw_power);
  }

  // b30 has shown a very strong current-like relationship in the CZ25 captures.
  if (this->outdoor_current_sensor_ != nullptr)
    this->outdoor_current_sensor_->publish_state(this->rx_buffer_[30] / 5.0f);
}

void PanasonicACCZ25::loop() {
  PanasonicAC::read_data();

  if (millis() - this->last_read_ > READ_TIMEOUT && !this->rx_buffer_.empty()) {
    this->log_packet(this->rx_buffer_);

    if (!this->verify_packet())
      return;

    this->waiting_for_response_ = false;
    this->last_packet_received_ = millis();

    if (this->rx_buffer_[0] == POLL_HEADER) {
      this->data = std::vector<uint8_t>(this->rx_buffer_.begin() + 2, this->rx_buffer_.begin() + 12);

      this->set_data(true);
      this->publish_cnt_telemetry_();

      if (this->rx_buffer_.size() > 12)
        this->action = this->determine_action_from_cnt_state_(this->rx_buffer_[12]);

      this->publish_state();

      if (this->state_ != ACState::Ready)
        this->state_ = ACState::Ready;
    } else {
      ESP_LOGD(TAG_CZ25, "Received unknown packet");
    }

    this->rx_buffer_.clear();
  }

  this->handle_cmd();
  this->handle_poll();
}

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
