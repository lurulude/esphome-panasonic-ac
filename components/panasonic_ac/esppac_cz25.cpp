#include "esppac_cnt.h"
#include "esppac_commands_cnt.h"

#include "esphome/core/log.h"

#include <cstdio>

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const char *const TAG_CZ25 = "panasonic_ac.cz25";
static const char *const TAG_SCAN = "panasonic_ac.cz25_scan";

static constexpr uint8_t PROBE_BYTES[] = {6, 7, 9};
static constexpr uint8_t PROBE_VALUES[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
static constexpr uint8_t PROBE_SAMPLE_COUNT = 3;
static constexpr uint32_t PROBE_SAMPLE_INTERVAL = 5000;
static constexpr uint32_t PROBE_BETWEEN_TESTS = 1000;
static constexpr uint32_t PROBE_RESPONSE_TIMEOUT = 3000;

std::string PanasonicACCZ25::determine_operational_state_(uint8_t state) const {
  // Captures from the CZ25 show that b12 describes the physical state machine,
  // independently of the selected mode in b2. AUTO/HEAT_COOL can therefore
  // report HEAT_* (0x4x), COOL_* (0x3x), and potentially DRY_* (0x2x) states.
  // b2 changes immediately after a command while b12 can remain in the old
  // physical family for several seconds. That is useful information and should
  // not be relabelled according to the newly selected mode.
  switch (state) {
    case 0x00:
      if (this->mode == climate::CLIMATE_MODE_OFF)
        return "OFF";
      if (this->mode == climate::CLIMATE_MODE_HEAT_COOL)
        return "AUTO_IDLE";
      return "IDLE_0x00";

    // 0x0x has been observed in AUTO captures, but its exact physical meaning
    // is not yet resolved. Keep the low-nibble phase hint without inventing a
    // heating/cooling direction.
    case 0x04:
      return this->mode == climate::CLIMATE_MODE_HEAT_COOL ? "AUTO_TRANS_0x04" : "STATE_0x04";
    case 0x08:
      return this->mode == climate::CLIMATE_MODE_HEAT_COOL ? "AUTO_START_0x08" : "STATE_0x08";
    case 0x0C:
      return this->mode == climate::CLIMATE_MODE_HEAT_COOL ? "AUTO_RUN_0x0C" : "STATE_0x0C";

    case 0x20:
      return "DRY_IDLE";
    case 0x24:
      return "DRY_TRANS";
    case 0x28:
      return "DRY_START";
    case 0x2C:
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
  // Across the observed state families, x8 is START and xC is RUN. x0 is
  // idle/base and x4 is a transition with the compressor treated as stopped.
  switch (state) {
    case 0x08:  // AUTO-family start; physical direction still unknown
    case 0x0C:  // AUTO-family run; observed, direction still unknown
    case 0x28:  // DRY start
    case 0x2C:  // DRY run
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
  // Keep normal climate semantics when the selected mode itself is OFF.
  if (this->mode == climate::CLIMATE_MODE_OFF)
    return climate::CLIMATE_ACTION_OFF;

  // Derive action primarily from the physical b12 family, not from selected
  // mode or current-vs-target temperature. This also preserves the real old
  // physical action for the few seconds after b2 has changed to a new mode.
  //
  // Low-nibble phase semantics from the captures:
  //   x0 = idle/base
  //   x4 = transition (not actively heating/cooling/drying)
  //   x8 = start
  //   xC = run
  switch (state) {
    case 0x00:
    case 0x04:
      return climate::CLIMATE_ACTION_IDLE;

    case 0x20:
    case 0x24:
      return climate::CLIMATE_ACTION_IDLE;
    case 0x28:
    case 0x2C:
      return climate::CLIMATE_ACTION_DRYING;

    case 0x30:
    case 0x34:
      return climate::CLIMATE_ACTION_IDLE;
    case 0x38:
    case 0x3C:
      return climate::CLIMATE_ACTION_COOLING;

    case 0x40:
    case 0x44:
      return climate::CLIMATE_ACTION_IDLE;
    case 0x48:
    case 0x4C:
      return climate::CLIMATE_ACTION_HEATING;

    case 0x60:
      return climate::CLIMATE_ACTION_FAN;
    default:
      break;
  }

  // 0x08/0x0C and any future unknown states do not yet reveal a trustworthy
  // heat/cool direction. Retain upstream temperature-based behavior only as a
  // fallback for those unresolved cases.
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

  if (this->selected_mode_raw_sensor_ != nullptr) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "0x%02X", this->rx_buffer_[2]);
    this->selected_mode_raw_sensor_->publish_state(buffer);
  }

  if (this->status_multiplex_sensor_ != nullptr && this->rx_buffer_.size() > 33) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X", this->rx_buffer_[31], this->rx_buffer_[32],
                  this->rx_buffer_[33]);
    this->status_multiplex_sensor_->publish_state(buffer);
  }

  if (this->compressor_running_sensor_ != nullptr)
    this->compressor_running_sensor_->publish_state(this->determine_compressor_running_(state));

  // b18 is Panasonic's primary indoor/current-temperature field on this CZ25.
  if (this->intake_temperature_sensor_ != nullptr && this->rx_buffer_[18] != 0x80)
    this->intake_temperature_sensor_->publish_state((int8_t) this->rx_buffer_[18]);

  // b21 is strongly linked to the same intake-air measurement on the tested CZ25,
  // but keep the entity name neutral until the exact processing relationship is known.
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

void PanasonicACCZ25::advance_probe_scan_() {
  this->probe_value_index_++;
  if (this->probe_value_index_ >= sizeof(PROBE_VALUES)) {
    this->probe_value_index_ = 0;
    this->probe_byte_index_++;
  }

  if (this->probe_byte_index_ >= sizeof(PROBE_BYTES)) {
    this->probe_phase_ = CZ25ProbePhase::IDLE;
    ESP_LOGI(TAG_SCAN, "CZ25_SCAN COMPLETE: tested payload bytes 6,7,9 with 00,01,02,04,08,10,20,40,80");
    return;
  }

  this->probe_phase_ = CZ25ProbePhase::SEND_PROBE;
  this->probe_next_action_ms_ = millis() + PROBE_BETWEEN_TESTS;
}

void PanasonicACCZ25::probe_scan_on_packet_() {
  if (!this->probe_scan_started_ || this->rx_buffer_.size() < 34)
    return;

  if (this->probe_phase_ == CZ25ProbePhase::WAIT_SAMPLE) {
    const uint8_t payload_index = PROBE_BYTES[this->probe_byte_index_];
    const uint8_t value = PROBE_VALUES[this->probe_value_index_];
    this->probe_sample_index_++;

    ESP_LOGI(TAG_SCAN,
             "CZ25_SCAN SAMPLE payload[%u]=0x%02X %u/%u | rx_p6=%02X rx_p7=%02X rx_p9=%02X | "
             "b2=%02X b12=%02X b13=%02X b16=%02X b17=%02X b18=%02X b21=%02X "
             "b28=%02X b29=%02X b30=%02X mux=%02X:%02X:%02X",
             payload_index, value, this->probe_sample_index_, PROBE_SAMPLE_COUNT, this->rx_buffer_[8],
             this->rx_buffer_[9], this->rx_buffer_[11], this->rx_buffer_[2], this->rx_buffer_[12],
             this->rx_buffer_[13], this->rx_buffer_[16], this->rx_buffer_[17], this->rx_buffer_[18],
             this->rx_buffer_[21], this->rx_buffer_[28], this->rx_buffer_[29], this->rx_buffer_[30],
             this->rx_buffer_[31], this->rx_buffer_[32], this->rx_buffer_[33]);

    if (this->probe_sample_index_ >= PROBE_SAMPLE_COUNT) {
      this->probe_phase_ = CZ25ProbePhase::SEND_RESTORE;
      this->probe_next_action_ms_ = millis();
    } else {
      this->probe_phase_ = CZ25ProbePhase::WAIT_POLL_DELAY;
      this->probe_next_action_ms_ = millis() + PROBE_SAMPLE_INTERVAL;
    }
  } else if (this->probe_phase_ == CZ25ProbePhase::WAIT_RESTORE) {
    const uint8_t payload_index = PROBE_BYTES[this->probe_byte_index_];
    const uint8_t value = PROBE_VALUES[this->probe_value_index_];
    ESP_LOGI(TAG_SCAN, "CZ25_SCAN RESTORED payload[%u] after test 0x%02X -> original 0x%02X", payload_index,
             value, this->probe_original_value_);
    this->advance_probe_scan_();
  }
}

bool PanasonicACCZ25::handle_probe_scan_() {
  if (!this->probe_scan_enabled_)
    return false;

  if (!this->probe_scan_started_) {
    if (this->state_ != ACState::Ready || !this->cmd.empty())
      return false;

    this->probe_scan_started_ = true;
    this->probe_byte_index_ = 0;
    this->probe_value_index_ = 0;
    this->probe_phase_ = CZ25ProbePhase::SEND_PROBE;
    this->probe_next_action_ms_ = millis() + 2000;
    ESP_LOGW(TAG_SCAN,
             "CZ25_SCAN START: one-shot control-payload scan enabled. Do not change AC settings until COMPLETE.");
    ESP_LOGI(TAG_SCAN, "CZ25_SCAN plan: payload bytes 6,7,9; values 00,01,02,04,08,10,20,40,80; 3 samples each");
    return true;
  }

  if (this->probe_phase_ == CZ25ProbePhase::IDLE)
    return false;

  if (this->probe_phase_ == CZ25ProbePhase::SEND_PROBE) {
    if (millis() < this->probe_next_action_ms_)
      return true;

    const uint8_t payload_index = PROBE_BYTES[this->probe_byte_index_];
    const uint8_t value = PROBE_VALUES[this->probe_value_index_];

    if (this->data.size() < 10) {
      ESP_LOGE(TAG_SCAN, "CZ25_SCAN aborted: current CNT payload is incomplete");
      this->probe_phase_ = CZ25ProbePhase::IDLE;
      return false;
    }

    this->probe_restore_command_ = this->data;
    this->probe_original_value_ = this->probe_restore_command_[payload_index];
    std::vector<uint8_t> probe_command = this->probe_restore_command_;
    probe_command[payload_index] = value;
    this->probe_sample_index_ = 0;

    ESP_LOGW(TAG_SCAN, "CZ25_SCAN TEST payload[%u]: original=0x%02X -> probe=0x%02X", payload_index,
             this->probe_original_value_, value);
    this->send_command(probe_command, CommandType::Normal, CTRL_HEADER);
    this->probe_phase_ = CZ25ProbePhase::WAIT_SAMPLE;
    return true;
  }

  if (this->probe_phase_ == CZ25ProbePhase::WAIT_SAMPLE) {
    if (millis() - this->last_packet_sent_ > PROBE_RESPONSE_TIMEOUT) {
      const uint8_t payload_index = PROBE_BYTES[this->probe_byte_index_];
      const uint8_t value = PROBE_VALUES[this->probe_value_index_];
      ESP_LOGW(TAG_SCAN, "CZ25_SCAN response timeout payload[%u]=0x%02X; continuing with explicit poll", payload_index,
               value);
      this->probe_phase_ = CZ25ProbePhase::WAIT_POLL_DELAY;
      this->probe_next_action_ms_ = millis() + 500;
    }
    return true;
  }

  if (this->probe_phase_ == CZ25ProbePhase::WAIT_POLL_DELAY) {
    if (millis() >= this->probe_next_action_ms_) {
      this->send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
      this->probe_phase_ = CZ25ProbePhase::WAIT_SAMPLE;
    }
    return true;
  }

  if (this->probe_phase_ == CZ25ProbePhase::SEND_RESTORE) {
    if (millis() < this->probe_next_action_ms_)
      return true;

    ESP_LOGI(TAG_SCAN, "CZ25_SCAN restoring payload[%u] to 0x%02X", PROBE_BYTES[this->probe_byte_index_],
             this->probe_original_value_);
    this->send_command(this->probe_restore_command_, CommandType::Normal, CTRL_HEADER);
    this->probe_phase_ = CZ25ProbePhase::WAIT_RESTORE;
    return true;
  }

  if (this->probe_phase_ == CZ25ProbePhase::WAIT_RESTORE) {
    if (millis() - this->last_packet_sent_ > PROBE_RESPONSE_TIMEOUT) {
      ESP_LOGW(TAG_SCAN, "CZ25_SCAN restore response timeout; advancing to next test");
      this->advance_probe_scan_();
    }
    return true;
  }

  return true;
}

void PanasonicACCZ25::loop() {
  PanasonicAC::read_data();

  if (millis() - this->last_read_ > READ_TIMEOUT && !this->rx_buffer_.empty()) {
    this->log_packet(this->rx_buffer_);

    if (!this->verify_packet()) {
      this->rx_buffer_.clear();
      return;
    }

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

      this->probe_scan_on_packet_();
    } else {
      ESP_LOGD(TAG_CZ25, "Received unknown packet");
    }

    this->rx_buffer_.clear();
  }

  if (!this->handle_probe_scan_()) {
    this->handle_cmd();
    this->handle_poll();
  }
}

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
