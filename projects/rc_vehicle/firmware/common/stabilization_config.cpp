#include "stabilization_config.hpp"

#include <cstdint>

namespace rc_vehicle {

bool StabilizationConfig::IsValid() const noexcept {
  return magic == kStabilizationConfigMagic && madgwick_beta > 0.0f &&
         madgwick_beta <= 1.0f && lpf_cutoff_hz >= 5.0f &&
         lpf_cutoff_hz <= 100.0f && imu_sample_rate_hz > 0.0f &&
         pid_kp >= 0.0f && pid_ki >= 0.0f && pid_kd >= 0.0f &&
         pid_max_correction > 0.0f && steer_to_yaw_rate_dps > 0.0f;
}

void StabilizationConfig::Reset() noexcept {
  enabled = false;
  madgwick_beta = 0.1f;
  lpf_cutoff_hz = 30.0f;
  imu_sample_rate_hz = 500.0f;
  mode = DriveMode::Normal;
  pid_kp = 0.1f;
  pid_ki = 0.0f;
  pid_kd = 0.005f;
  pid_max_integral = 0.5f;
  pid_max_correction = 0.3f;
  steer_to_yaw_rate_dps = 90.0f;
  fade_ms = 500;
  pitch_comp_enabled = false;
  pitch_comp_gain = 0.01f;
  pitch_comp_max_correction = 0.25f;
  slip_target_deg = 0.0f;
  slip_kp = 0.0f;
  slip_ki = 0.0f;
  slip_kd = 0.0f;
  slip_max_integral = 5.0f;
  slip_max_correction = 0.0f;
  adaptive_pid_enabled = false;
  adaptive_speed_ref_ms = 1.5f;
  adaptive_scale_min = 0.5f;
  adaptive_scale_max = 2.0f;
  oversteer_warn_enabled = false;
  oversteer_slip_thresh_deg = 20.0f;
  oversteer_rate_thresh_deg_s = 50.0f;
  oversteer_throttle_reduction = 0.0f;
  version = 1;
  magic = kStabilizationConfigMagic;
}

void StabilizationConfig::ApplyModeDefaults() noexcept {
  switch (mode) {
    case DriveMode::Sport:  // быстрый отклик, лёгкий slip assist
      pid_kp = 0.20f;
      pid_ki = 0.01f;
      pid_kd = 0.010f;
      pid_max_integral = 1.0f;
      pid_max_correction = 0.40f;
      steer_to_yaw_rate_dps = 120.0f;
      pitch_comp_gain = 0.02f;
      pitch_comp_max_correction = 0.30f;
      slip_target_deg = 5.0f;
      slip_kp = 0.003f;
      slip_ki = 0.0f;
      slip_kd = 0.001f;
      slip_max_integral = 5.0f;
      slip_max_correction = 0.15f;
      break;
    case DriveMode::Drift:  // мягкая коррекция yaw, slip angle PID включён
      pid_kp = 0.05f;
      pid_ki = 0.00f;
      pid_kd = 0.002f;
      pid_max_integral = 0.3f;
      pid_max_correction = 0.20f;
      steer_to_yaw_rate_dps = 60.0f;
      pitch_comp_gain = 0.005f;
      pitch_comp_max_correction = 0.15f;
      slip_target_deg = 15.0f;
      slip_kp = 0.008f;
      slip_ki = 0.0f;
      slip_kd = 0.002f;
      slip_max_integral = 5.0f;
      slip_max_correction = 0.25f;
      break;
    default:  // Normal: базовые параметры, slip PID выключен
      pid_kp = 0.10f;
      pid_ki = 0.00f;
      pid_kd = 0.005f;
      pid_max_integral = 0.5f;
      pid_max_correction = 0.30f;
      steer_to_yaw_rate_dps = 90.0f;
      pitch_comp_gain = 0.01f;
      pitch_comp_max_correction = 0.25f;
      slip_target_deg = 0.0f;
      slip_kp = 0.0f;
      slip_ki = 0.0f;
      slip_kd = 0.0f;
      slip_max_integral = 5.0f;
      slip_max_correction = 0.0f;
      break;
  }
}

void StabilizationConfig::Clamp() noexcept {
  if (madgwick_beta < 0.01f) madgwick_beta = 0.01f;
  if (madgwick_beta > 1.0f) madgwick_beta = 1.0f;
  if (lpf_cutoff_hz < 5.0f) lpf_cutoff_hz = 5.0f;
  if (lpf_cutoff_hz > 100.0f) lpf_cutoff_hz = 100.0f;
  if (imu_sample_rate_hz < 100.0f) imu_sample_rate_hz = 100.0f;
  if (static_cast<uint8_t>(mode) > 2) mode = DriveMode::Normal;
  if (pid_kp < 0.0f) pid_kp = 0.0f;
  if (pid_ki < 0.0f) pid_ki = 0.0f;
  if (pid_kd < 0.0f) pid_kd = 0.0f;
  if (pid_max_integral < 0.0f) pid_max_integral = 0.0f;
  if (pid_max_correction < 0.0f) pid_max_correction = 0.0f;
  if (pid_max_correction > 1.0f) pid_max_correction = 1.0f;
  if (steer_to_yaw_rate_dps < 10.0f) steer_to_yaw_rate_dps = 10.0f;
  if (steer_to_yaw_rate_dps > 360.0f) steer_to_yaw_rate_dps = 360.0f;
  if (fade_ms > 5000) fade_ms = 5000;
  if (pitch_comp_gain < 0.0f) pitch_comp_gain = 0.0f;
  if (pitch_comp_gain > 0.05f) pitch_comp_gain = 0.05f;
  if (pitch_comp_max_correction < 0.0f) pitch_comp_max_correction = 0.0f;
  if (pitch_comp_max_correction > 0.5f) pitch_comp_max_correction = 0.5f;
  if (slip_target_deg < -45.0f) slip_target_deg = -45.0f;
  if (slip_target_deg > 45.0f) slip_target_deg = 45.0f;
  if (slip_kp < 0.0f) slip_kp = 0.0f;
  if (slip_kp > 1.0f) slip_kp = 1.0f;
  if (slip_ki < 0.0f) slip_ki = 0.0f;
  if (slip_kd < 0.0f) slip_kd = 0.0f;
  if (slip_max_integral < 0.0f) slip_max_integral = 0.0f;
  if (slip_max_correction < 0.0f) slip_max_correction = 0.0f;
  if (slip_max_correction > 1.0f) slip_max_correction = 1.0f;
  // Adaptive PID
  if (adaptive_speed_ref_ms < 0.1f) adaptive_speed_ref_ms = 0.1f;
  if (adaptive_speed_ref_ms > 10.0f) adaptive_speed_ref_ms = 10.0f;
  if (adaptive_scale_min < 0.1f) adaptive_scale_min = 0.1f;
  if (adaptive_scale_min > 1.0f) adaptive_scale_min = 1.0f;
  if (adaptive_scale_max < 1.0f) adaptive_scale_max = 1.0f;
  if (adaptive_scale_max > 5.0f) adaptive_scale_max = 5.0f;
  // Oversteer warning
  if (oversteer_slip_thresh_deg < 5.0f) oversteer_slip_thresh_deg = 5.0f;
  if (oversteer_slip_thresh_deg > 45.0f) oversteer_slip_thresh_deg = 45.0f;
  if (oversteer_rate_thresh_deg_s < 10.0f) oversteer_rate_thresh_deg_s = 10.0f;
  if (oversteer_rate_thresh_deg_s > 500.0f)
    oversteer_rate_thresh_deg_s = 500.0f;
  if (oversteer_throttle_reduction < 0.0f) oversteer_throttle_reduction = 0.0f;
  if (oversteer_throttle_reduction > 1.0f) oversteer_throttle_reduction = 1.0f;
}

}  // namespace rc_vehicle
