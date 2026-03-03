#include "stabilization_config_json.hpp"

using rc_vehicle::DriveMode;
using rc_vehicle::StabilizationConfig;

cJSON* StabilizationConfigToJson(const StabilizationConfig& cfg) {
  cJSON* obj = cJSON_CreateObject();
  if (!obj) return nullptr;

  // Общие параметры
  cJSON_AddBoolToObject(obj, "enabled", cfg.enabled);
  cJSON_AddNumberToObject(obj, "madgwick_beta", cfg.madgwick_beta);
  cJSON_AddNumberToObject(obj, "lpf_cutoff_hz", cfg.lpf_cutoff_hz);
  cJSON_AddNumberToObject(obj, "imu_sample_rate_hz", cfg.imu_sample_rate_hz);
  cJSON_AddNumberToObject(obj, "mode", static_cast<uint8_t>(cfg.mode));

  // Yaw rate PID
  cJSON_AddNumberToObject(obj, "pid_kp", cfg.pid_kp);
  cJSON_AddNumberToObject(obj, "pid_ki", cfg.pid_ki);
  cJSON_AddNumberToObject(obj, "pid_kd", cfg.pid_kd);
  cJSON_AddNumberToObject(obj, "pid_max_integral", cfg.pid_max_integral);
  cJSON_AddNumberToObject(obj, "pid_max_correction", cfg.pid_max_correction);
  cJSON_AddNumberToObject(obj, "steer_to_yaw_rate_dps", cfg.steer_to_yaw_rate_dps);
  cJSON_AddNumberToObject(obj, "fade_ms", cfg.fade_ms);

  // Pitch compensation (slope stabilization)
  cJSON_AddBoolToObject(obj, "pitch_comp_enabled", cfg.pitch_comp_enabled);
  cJSON_AddNumberToObject(obj, "pitch_comp_gain", cfg.pitch_comp_gain);
  cJSON_AddNumberToObject(obj, "pitch_comp_max_correction",
                          cfg.pitch_comp_max_correction);

  // Slip angle PID (drift mode)
  cJSON_AddNumberToObject(obj, "slip_target_deg", cfg.slip_target_deg);
  cJSON_AddNumberToObject(obj, "slip_kp", cfg.slip_kp);
  cJSON_AddNumberToObject(obj, "slip_ki", cfg.slip_ki);
  cJSON_AddNumberToObject(obj, "slip_kd", cfg.slip_kd);
  cJSON_AddNumberToObject(obj, "slip_max_integral", cfg.slip_max_integral);
  cJSON_AddNumberToObject(obj, "slip_max_correction", cfg.slip_max_correction);

  // Adaptive PID (Phase 4.1)
  cJSON_AddBoolToObject(obj, "adaptive_pid_enabled", cfg.adaptive_pid_enabled);
  cJSON_AddNumberToObject(obj, "adaptive_speed_ref_ms", cfg.adaptive_speed_ref_ms);
  cJSON_AddNumberToObject(obj, "adaptive_scale_min", cfg.adaptive_scale_min);
  cJSON_AddNumberToObject(obj, "adaptive_scale_max", cfg.adaptive_scale_max);

  // Oversteer warning (Phase 4.2)
  cJSON_AddBoolToObject(obj, "oversteer_warn_enabled", cfg.oversteer_warn_enabled);
  cJSON_AddNumberToObject(obj, "oversteer_slip_thresh_deg",
                          cfg.oversteer_slip_thresh_deg);
  cJSON_AddNumberToObject(obj, "oversteer_rate_thresh_deg_s",
                          cfg.oversteer_rate_thresh_deg_s);
  cJSON_AddNumberToObject(obj, "oversteer_throttle_reduction",
                          cfg.oversteer_throttle_reduction);

  return obj;
}

void StabilizationConfigFromJson(StabilizationConfig& cfg, const cJSON* json) {
  auto get_bool = [json](const char* key, bool& field) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (item && cJSON_IsBool(item)) field = cJSON_IsTrue(item);
  };
  auto get_float = [json](const char* key, float& field) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (item && cJSON_IsNumber(item))
      field = static_cast<float>(item->valuedouble);
  };
  auto get_drive_mode = [json](const char* key, DriveMode& field) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (item && cJSON_IsNumber(item))
      field = static_cast<DriveMode>(item->valueint);
  };
  auto get_uint32 = [json](const char* key, uint32_t& field) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (item && cJSON_IsNumber(item))
      field = static_cast<uint32_t>(item->valueint);
  };

  // Общие параметры
  get_bool("enabled", cfg.enabled);
  get_float("madgwick_beta", cfg.madgwick_beta);
  get_float("lpf_cutoff_hz", cfg.lpf_cutoff_hz);
  get_float("imu_sample_rate_hz", cfg.imu_sample_rate_hz);
  get_drive_mode("mode", cfg.mode);

  // Yaw rate PID
  get_float("pid_kp", cfg.pid_kp);
  get_float("pid_ki", cfg.pid_ki);
  get_float("pid_kd", cfg.pid_kd);
  get_float("pid_max_integral", cfg.pid_max_integral);
  get_float("pid_max_correction", cfg.pid_max_correction);
  get_float("steer_to_yaw_rate_dps", cfg.steer_to_yaw_rate_dps);
  get_uint32("fade_ms", cfg.fade_ms);

  // Pitch compensation
  get_bool("pitch_comp_enabled", cfg.pitch_comp_enabled);
  get_float("pitch_comp_gain", cfg.pitch_comp_gain);
  get_float("pitch_comp_max_correction", cfg.pitch_comp_max_correction);

  // Slip angle PID
  get_float("slip_target_deg", cfg.slip_target_deg);
  get_float("slip_kp", cfg.slip_kp);
  get_float("slip_ki", cfg.slip_ki);
  get_float("slip_kd", cfg.slip_kd);
  get_float("slip_max_integral", cfg.slip_max_integral);
  get_float("slip_max_correction", cfg.slip_max_correction);

  // Adaptive PID
  get_bool("adaptive_pid_enabled", cfg.adaptive_pid_enabled);
  get_float("adaptive_speed_ref_ms", cfg.adaptive_speed_ref_ms);
  get_float("adaptive_scale_min", cfg.adaptive_scale_min);
  get_float("adaptive_scale_max", cfg.adaptive_scale_max);

  // Oversteer warning
  get_bool("oversteer_warn_enabled", cfg.oversteer_warn_enabled);
  get_float("oversteer_slip_thresh_deg", cfg.oversteer_slip_thresh_deg);
  get_float("oversteer_rate_thresh_deg_s", cfg.oversteer_rate_thresh_deg_s);
  get_float("oversteer_throttle_reduction", cfg.oversteer_throttle_reduction);
}
