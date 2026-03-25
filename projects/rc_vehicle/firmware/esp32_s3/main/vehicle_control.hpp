#pragma once

/**
 * @file vehicle_control.hpp
 * @brief ESP32-совместимый API управления машиной
 *
 * Обёртки над глобальным экземпляром rc_vehicle::VehicleControlUnified.
 * Экземпляр создаётся в этом модуле (ESP32-слой владеет lifetime).
 */

#include "esp_err.h"
#include "telemetry_log.hpp"
#include "vehicle_control_platform_esp32.hpp"
#include "vehicle_control_unified.hpp"

namespace detail {
inline rc_vehicle::VehicleControlUnified& GetVehicleControlImpl() {
  static rc_vehicle::VehicleControlUnified instance;
  return instance;
}
inline rc_vehicle::IVehicleControl& GetVehicleControl() {
  return GetVehicleControlImpl();
}
}  // namespace detail

inline esp_err_t VehicleControlInit(void) {
  auto& vc = detail::GetVehicleControlImpl();
  static bool platform_set = false;
  if (!platform_set) {
    auto platform = std::make_unique<rc_vehicle::VehicleControlPlatformEsp32>();
    vc.SetPlatform(std::move(platform));
    platform_set = true;
  }
  auto result = vc.Init();
  return (result == rc_vehicle::PlatformError::Ok) ? ESP_OK : ESP_FAIL;
}

inline void VehicleControlOnWifiCommand(float throttle, float steering) {
  detail::GetVehicleControl().OnWifiCommand(throttle, steering);
}

inline void VehicleControlStartCalibration(bool full) {
  detail::GetVehicleControl().StartCalibration(full);
}

inline bool VehicleControlStartForwardCalibration(void) {
  return detail::GetVehicleControl().StartForwardCalibration();
}

inline bool VehicleControlStartAutoForwardCalibration(
    float target_accel_g = 0.1f) {
  return detail::GetVehicleControl().StartAutoForwardCalibration(target_accel_g);
}

inline const char* VehicleControlGetCalibStatus(void) {
  return detail::GetVehicleControl().GetCalibStatus();
}

inline int VehicleControlGetCalibStage(void) {
  return detail::GetVehicleControl().GetCalibStage();
}

inline void VehicleControlSetForwardDirection(float fx, float fy, float fz) {
  detail::GetVehicleControl().SetForwardDirection(fx, fy, fz);
}

inline rc_vehicle::StabilizationConfig VehicleControlGetStabilizationConfig(
    void) {
  return detail::GetVehicleControl().GetStabilizationConfig();
}

inline bool VehicleControlSetStabilizationConfig(
    const rc_vehicle::StabilizationConfig& config, bool save_to_nvs = true) {
  return detail::GetVehicleControl().SetStabilizationConfig(config,
                                                            save_to_nvs);
}

inline void VehicleControlGetLogInfo(size_t* count_out, size_t* cap_out) {
  size_t cnt = 0, cap = 0;
  detail::GetVehicleControl().GetLogInfo(cnt, cap);
  if (count_out) *count_out = cnt;
  if (cap_out) *cap_out = cap;
}

inline bool VehicleControlGetLogFrame(size_t idx, TelemetryLogFrame* out) {
  if (!out) return false;
  return detail::GetVehicleControl().GetLogFrame(idx, *out);
}

inline void VehicleControlSetKidsModeActive(bool active) {
  detail::GetVehicleControl().SetKidsModeActive(active);
}

inline bool VehicleControlIsKidsModeActive(void) {
  return detail::GetVehicleControl().IsKidsModeActive();
}

inline void VehicleControlClearLog() {
  detail::GetVehicleControl().ClearLog();
}

inline bool VehicleControlStartSteeringTrimCalibration(
    float target_accel_g = 0.1f) {
  return detail::GetVehicleControl().StartSteeringTrimCalibration(
      target_accel_g);
}

inline void VehicleControlStopSteeringTrimCalibration() {
  detail::GetVehicleControl().StopSteeringTrimCalibration();
}

inline bool VehicleControlIsSteeringTrimCalibActive() {
  return detail::GetVehicleControl().IsSteeringTrimCalibActive();
}

inline rc_vehicle::SteeringTrimCalibration::Result
VehicleControlGetSteeringTrimCalibResult() {
  return detail::GetVehicleControl().GetSteeringTrimCalibResult();
}

inline bool VehicleControlStartComOffsetCalibration(
    float target_accel_g = 0.1f, float steering_magnitude = 0.5f,
    float cruise_duration_sec = 5.0f) {
  return detail::GetVehicleControl().StartComOffsetCalibration(
      target_accel_g, steering_magnitude, cruise_duration_sec);
}

inline void VehicleControlStopComOffsetCalibration() {
  detail::GetVehicleControl().StopComOffsetCalibration();
}

inline bool VehicleControlIsComOffsetCalibActive() {
  return detail::GetVehicleControl().IsComOffsetCalibActive();
}

inline rc_vehicle::ComOffsetCalibration::Result
VehicleControlGetComOffsetCalibResult() {
  return detail::GetVehicleControl().GetComOffsetCalibResult();
}

inline bool VehicleControlStartTest(const rc_vehicle::TestParams& params) {
  return detail::GetVehicleControl().StartTest(params);
}

inline void VehicleControlStopTest() {
  detail::GetVehicleControl().StopTest();
}

inline bool VehicleControlIsTestActive() {
  return detail::GetVehicleControl().IsTestActive();
}

inline rc_vehicle::TestRunner::Status VehicleControlGetTestStatus() {
  return detail::GetVehicleControl().GetTestStatus();
}

inline std::vector<rc_vehicle::SelfTestItem> VehicleControlRunSelfTest() {
  return detail::GetVehicleControl().RunSelfTest();
}

inline bool VehicleControlIsReady() {
  return detail::GetVehicleControl().IsReady();
}
