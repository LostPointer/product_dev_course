#pragma once

#include "cJSON.h"
#include "esp_http_server.h"

namespace rc_vehicle {

/**
 * @brief WebSocket command handlers
 *
 * These functions handle specific WebSocket JSON commands.
 * Each handler is responsible for:
 * 1. Parsing command-specific parameters from JSON
 * 2. Executing the command logic
 * 3. Sending a JSON response back to the client
 */

/**
 * @brief Handle IMU calibration command
 *
 * Supports three modes:
 * - "gyro": Calibrate gyroscope only
 * - "full": Full calibration (stage 1: stationary)
 * - "forward": Forward calibration (stage 2: moving forward)
 *
 * Request: {"type":"calibrate_imu","mode":"gyro"|"full"|"forward"}
 * Response:
 * {"type":"calibrate_imu_ack","status":"collecting"|"failed","stage":1|2,"ok":true|false}
 */
void HandleCalibrateImu(cJSON* json, httpd_req_t* req);

/**
 * @brief Get calibration status
 *
 * Request: {"type":"get_calib_status"}
 * Response: {"type":"calib_status","status":"...","stage":1|2}
 */
void HandleGetCalibStatus(cJSON* json, httpd_req_t* req);

/**
 * @brief Set forward direction vector
 *
 * Request: {"type":"set_forward_direction","vec":[fx,fy,fz]}
 * Response: {"type":"set_forward_direction_ack","ok":true}
 */
void HandleSetForwardDirection(cJSON* json, httpd_req_t* req);

/**
 * @brief Get stabilization configuration
 *
 * Request: {"type":"get_stab_config"}
 * Response: {"type":"stab_config",...config fields...}
 */
void HandleGetStabConfig(cJSON* json, httpd_req_t* req);

/**
 * @brief Set stabilization configuration
 *
 * Request: {"type":"set_stab_config",...config fields...}
 * Response: {"type":"set_stab_config_ack","ok":true,...applied config...}
 */
void HandleSetStabConfig(cJSON* json, httpd_req_t* req);

/**
 * @brief Get telemetry log information
 *
 * Request: {"type":"get_log_info"}
 * Response: {"type":"log_info","count":N,"capacity":M}
 */
void HandleGetLogInfo(cJSON* json, httpd_req_t* req);

/**
 * @brief Get telemetry log data
 *
 * Request: {"type":"get_log_data","offset":N,"count":M}
 * Response: {"type":"log_data","frames":[...]}
 */
void HandleGetLogData(cJSON* json, httpd_req_t* req);

/**
 * @brief Clear telemetry log
 *
 * Request: {"type":"clear_log"}
 * Response: {"type":"clear_log_ack","ok":true}
 */
void HandleClearLog(cJSON* json, httpd_req_t* req);

/**
 * @brief Set Kids Mode preset
 *
 * Request: {"type":"set_kids_preset","preset":0|1|2|3}
 * Presets: 0=Custom, 1=Toddler(3-5y), 2=Child(6-9y), 3=Preteen(10-12y)
 * Response: {"type":"set_kids_preset_ack","ok":true,...applied config...}
 */
void HandleSetKidsPreset(cJSON* json, httpd_req_t* req);

/**
 * @brief Get available Kids Mode presets
 *
 * Request: {"type":"get_kids_presets"}
 * Response: {"type":"kids_presets","presets":[...]}
 */
void HandleGetKidsPresets(cJSON* json, httpd_req_t* req);

/**
 * @brief Toggle Kids Mode on/off
 *
 * Request: {"type":"toggle_kids_mode","active":true|false}
 * Response: {"type":"toggle_kids_mode_ack","active":true|false}
 */
void HandleToggleKidsMode(cJSON* json, httpd_req_t* req);

/**
 * @brief Start steering trim auto-calibration
 *
 * Drives straight, measures yaw rate drift, computes optimal trim.
 *
 * Request: {"type":"calibrate_steering_trim","target_accel":0.1}
 * Response: {"type":"calibrate_steering_trim_ack","ok":true,"status":"started"}
 */
void HandleCalibrateSteeringTrim(cJSON* json, httpd_req_t* req);

/**
 * @brief Get steering trim calibration status/result
 *
 * Request: {"type":"get_steering_trim_status"}
 * Response: {"type":"steering_trim_status","active":bool,"phase":"...","result":{...}}
 */
void HandleGetSteeringTrimStatus(cJSON* json, httpd_req_t* req);

/**
 * @brief Start CoM offset calibration (circular CW+CCW)
 *
 * Request: {"type":"calibrate_com_offset","target_accel":0.1,"steering":0.5,"duration":5.0}
 * Response: {"type":"calibrate_com_offset_ack","ok":true,"status":"started"}
 */
void HandleCalibrateComOffset(cJSON* json, httpd_req_t* req);

/**
 * @brief Get CoM offset calibration status/result
 *
 * Request: {"type":"get_com_offset_status"}
 * Response: {"type":"com_offset_status","active":bool,"result":{...}}
 */
void HandleGetComOffsetStatus(cJSON* json, httpd_req_t* req);

/**
 * @brief Start automated test maneuver
 *
 * Request: {"type":"start_test","test_type":"straight"|"circle"|"step",
 *           "target_accel":0.1,"duration":3.0,"steering":0.5}
 * Response: {"type":"start_test_ack","ok":true,"test_type":"..."}
 */
void HandleStartTest(cJSON* json, httpd_req_t* req);

/**
 * @brief Stop running test maneuver
 *
 * Request: {"type":"stop_test"}
 * Response: {"type":"stop_test_ack","ok":true}
 */
void HandleStopTest(cJSON* json, httpd_req_t* req);

/**
 * @brief Get test maneuver status
 *
 * Request: {"type":"get_test_status"}
 * Response: {"type":"test_status","active":bool,"phase":"...","type":"...","elapsed":N,"valid":bool}
 */
void HandleGetTestStatus(cJSON* json, httpd_req_t* req);

/**
 * @brief Run self-test (hardware diagnostics)
 *
 * Checks 10 subsystems: control loop, IMU, gyro, accel, Madgwick,
 * EKF ZUPT, failsafe, calibration, telemetry log, PWM.
 *
 * Request: {"type":"run_self_test"}
 * Response: {"type":"self_test_result","passed":bool,"tests":[...]}
 */
void HandleRunSelfTest(cJSON* json, httpd_req_t* req);

/**
 * @brief Start UDP telemetry streaming
 *
 * Request: {"type":"udp_stream_start","ip":"192.168.4.100","port":5555,"hz":100}
 * Response: {"type":"udp_stream_start_ack","ok":true,"ip":"...","port":N,"hz":N}
 */
void HandleUdpStreamStart(cJSON* json, httpd_req_t* req);

/**
 * @brief Stop UDP telemetry streaming
 *
 * Request: {"type":"udp_stream_stop"}
 * Response: {"type":"udp_stream_stop_ack","ok":true}
 */
void HandleUdpStreamStop(cJSON* json, httpd_req_t* req);

/**
 * @brief Get UDP telemetry streaming status
 *
 * Request: {"type":"udp_stream_status"}
 * Response: {"type":"udp_stream_status","streaming":bool,...}
 */
void HandleUdpStreamStatus(cJSON* json, httpd_req_t* req);

}  // namespace rc_vehicle