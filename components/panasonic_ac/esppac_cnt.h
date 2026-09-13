#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esppac.h"

#include <string>

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const uint8_t CTRL_HEADER = 0xF0;  // The header for control frames
static const uint8_t POLL_HEADER = 0x70;  // The header for the poll command

static const int POLL_INTERVAL = 5000;  // The interval at which to poll the AC
static const int CMD_INTERVAL = 250;    // The interval at which to send commands

enum class ACState {
  Initializing,  // Before first query response is receive
  Ready,         // All done, ready to receive regular packets
};

class PanasonicACCNT : public PanasonicAC {
 public:
  void control(const climate::ClimateCall &call) override;

  void on_horizontal_swing_change(const StringRef &swing) override;
  void on_vertical_swing_change(const StringRef &swing) override;
  void on_nanoex_change(bool nanoex) override;
  void on_eco_change(bool eco) override;
  void on_econavi_change(bool eco) override;
  void on_mild_dry_change(bool mild_dry) override;

  void setup() override;
  void loop() override;

 protected:
  ACState state_ = ACState::Initializing;  // Stores the internal state of the AC, used during initialization

  // uint8_t data[10];
  std::vector<uint8_t> data = std::vector<uint8_t>(10);  // Stores the data received from the AC
  std::vector<uint8_t> cmd;                              // Used to build next command

  void handle_poll();
  void handle_cmd();

  void set_data(bool set);

  void send_command(std::vector<uint8_t> command, CommandType type, uint8_t header);
  void send_packet(const std::vector<uint8_t> &command, CommandType type);

  bool verify_packet();
  void handle_packet();
};

class PanasonicACCZ25 : public PanasonicACCNT {
 public:
  void loop() override;

  void set_operational_state_sensor(text_sensor::TextSensor *sensor) { this->operational_state_sensor_ = sensor; }
  void set_operational_state_raw_sensor(text_sensor::TextSensor *sensor) { this->operational_state_raw_sensor_ = sensor; }
  void set_selected_mode_raw_sensor(text_sensor::TextSensor *sensor) { this->selected_mode_raw_sensor_ = sensor; }
  void set_status_multiplex_sensor(text_sensor::TextSensor *sensor) { this->status_multiplex_sensor_ = sensor; }
  void set_raw_status_packet_sensor(text_sensor::TextSensor *sensor) { this->raw_status_packet_sensor_ = sensor; }
  void set_compressor_running_sensor(binary_sensor::BinarySensor *sensor) { this->compressor_running_sensor_ = sensor; }
  void set_intake_temperature_sensor(sensor::Sensor *sensor) { this->intake_temperature_sensor_ = sensor; }
  void set_temperature_b21_sensor(sensor::Sensor *sensor) { this->temperature_b21_sensor_ = sensor; }
  void set_control_reference_sensor(sensor::Sensor *sensor) { this->control_reference_sensor_ = sensor; }
  void set_outdoor_power_raw_sensor(sensor::Sensor *sensor) { this->outdoor_power_raw_sensor_ = sensor; }
  void set_outdoor_current_sensor(sensor::Sensor *sensor) { this->outdoor_current_sensor_ = sensor; }

 protected:
  text_sensor::TextSensor *operational_state_sensor_ = nullptr;
  text_sensor::TextSensor *operational_state_raw_sensor_ = nullptr;
  text_sensor::TextSensor *selected_mode_raw_sensor_ = nullptr;
  text_sensor::TextSensor *status_multiplex_sensor_ = nullptr;
  text_sensor::TextSensor *raw_status_packet_sensor_ = nullptr;
  binary_sensor::BinarySensor *compressor_running_sensor_ = nullptr;
  sensor::Sensor *intake_temperature_sensor_ = nullptr;
  sensor::Sensor *temperature_b21_sensor_ = nullptr;
  sensor::Sensor *control_reference_sensor_ = nullptr;
  sensor::Sensor *outdoor_power_raw_sensor_ = nullptr;
  sensor::Sensor *outdoor_current_sensor_ = nullptr;

  void publish_cnt_telemetry_();
  std::string determine_operational_state_(uint8_t state) const;
  bool determine_compressor_running_(uint8_t state) const;
  climate::ClimateAction determine_action_from_cnt_state_(uint8_t state);
};

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
