#pragma once

#include <cstdint>

/**
 * @brief Конфигурация системы стабилизации
 *
 * Параметры фильтрации и режимов стабилизации, сохраняемые в NVS.
 * Используется для настройки Madgwick AHRS и LPF Butterworth через WebSocket
 * API.
 */
struct StabilizationConfig {
  /** Включена ли стабилизация (по умолчанию выключена) */
  bool enabled{false};

  /**
   * Коэффициент коррекции Madgwick (beta).
   * Диапазон: 0.01–1.0, по умолчанию 0.1.
   * Больше значение → быстрее реакция на акселерометр, но больше шум.
   * Меньше значение → медленнее реакция, но стабильнее ориентация.
   */
  float madgwick_beta{0.1f};

  /**
   * Частота среза LPF Butterworth для gyro Z (Hz).
   * Диапазон: 5–100 Hz, по умолчанию 30 Hz.
   * Используется для фильтрации угловой скорости перед ПИД контролем рыскания.
   * Меньше → сильнее фильтрация (меньше шум, но больше задержка).
   * Больше → слабее фильтрация (быстрее отклик, но больше шум).
   */
  float lpf_cutoff_hz{30.0f};

  /**
   * Частота дискретизации IMU (Hz).
   * По умолчанию 500 Hz (период 2 мс).
   * Используется для настройки LPF Butterworth.
   */
  float imu_sample_rate_hz{500.0f};

  /**
   * Режим стабилизации (для будущего расширения).
   * 0 = normal (базовый контроль рыскания)
   * 1 = sport (более агрессивные параметры)
   * 2 = drift (управление дрифтом)
   */
  uint8_t mode{0};

  // ── ПИД-регулятор yaw rate ──────────────────────────────────────────────

  /** Пропорциональный коэффициент ПИД */
  float pid_kp{0.1f};

  /** Интегральный коэффициент ПИД */
  float pid_ki{0.0f};

  /** Дифференциальный коэффициент ПИД */
  float pid_kd{0.005f};

  /**
   * Anti-windup: ограничение накопителя интегратора.
   * Единицы: deg/s (т.к. ошибка в deg/s).
   */
  float pid_max_integral{0.5f};

  /**
   * Максимальная поправка руля от ПИД [-1..1].
   * Ограничивает влияние регулятора на команду руления.
   */
  float pid_max_correction{0.3f};

  /**
   * Масштаб: steer_command [-1..1] → желаемая угловая скорость (deg/s).
   * При steer=1.0 желаемая угловая скорость = steer_to_yaw_rate_dps.
   * Диапазон: 10–360 deg/s.
   */
  float steer_to_yaw_rate_dps{90.0f};

  /**
   * Время нарастания/спада веса стабилизации (мс).
   * При включении/выключении стабилизации поправка руля плавно
   * нарастает/убывает за это время. 0 = мгновенное переключение.
   * Диапазон: 0–5000 мс, по умолчанию 500 мс.
   */
  uint32_t fade_ms{500};

  // ── Pitch compensation (стабилизация на склонах) ───────────────────────

  /**
   * Включена ли компенсация наклона (pitch).
   * При включении корректирует газ в зависимости от угла наклона.
   */
  bool pitch_comp_enabled{false};

  /**
   * Коэффициент pitch → throttle (дельта газа на градус наклона).
   * Положительное значение: наклон носом вверх (подъём) → увеличить газ.
   * Диапазон: 0.0–0.05, по умолчанию 0.01 (1% газа на градус).
   */
  float pitch_comp_gain{0.01f};

  /**
   * Максимальная поправка газа от компенсации наклона [0..1].
   * Ограничивает влияние алгоритма на команду газа.
   * Диапазон: 0.0–0.5, по умолчанию 0.25.
   */
  float pitch_comp_max_correction{0.25f};

  // ── Slip angle PID (управление дрифтом, mode=2) ─────────────────────────

  /**
   * Целевой угол заноса (градусы).
   * Диапазон: -45..45, по умолчанию 0 (нет заноса).
   * Активен только в drift mode (mode=2).
   */
  float slip_target_deg{0.0f};

  /** Пропорциональный коэффициент ПИД slip angle → throttle */
  float slip_kp{0.0f};

  /** Интегральный коэффициент ПИД slip angle */
  float slip_ki{0.0f};

  /** Дифференциальный коэффициент ПИД slip angle */
  float slip_kd{0.0f};

  /** Anti-windup: ограничение накопителя интегратора (градусы·с) */
  float slip_max_integral{5.0f};

  /**
   * Максимальная поправка газа от slip PID [0..1].
   * По умолчанию 0 — PID выключен.
   */
  float slip_max_correction{0.0f};

  // ── Адаптивное масштабирование ПИД по скорости (EKF) ────────────────────

  /**
   * Включить адаптивное масштабирование yaw PID по скорости из EKF.
   * При включении выход ПИД умножается на (speed / speed_ref), зажатый в
   * [scale_min..scale_max].
   */
  bool adaptive_pid_enabled{false};

  /**
   * Эталонная скорость для адаптивного ПИД [м/с].
   * При speed == speed_ref масштаб равен 1.0.
   * Диапазон: 0.1–10 m/s.
   */
  float adaptive_speed_ref_ms{1.5f};

  /**
   * Минимальный коэффициент масштабирования ПИД (при низкой скорости).
   * Диапазон: 0.1–1.0.
   */
  float adaptive_scale_min{0.5f};

  /**
   * Максимальный коэффициент масштабирования ПИД (при высокой скорости).
   * Диапазон: 1.0–5.0.
   */
  float adaptive_scale_max{2.0f};

  // ── Предсказание заноса (oversteer warning) ──────────────────────────────

  /**
   * Включить предупреждение о заносе (oversteer prediction).
   * При превышении порогов slip angle и скорости его изменения
   * устанавливается флаг oversteer и опционально снижается газ.
   */
  bool oversteer_warn_enabled{false};

  /**
   * Порог модуля угла заноса для срабатывания oversteer [градусы].
   * Диапазон: 5–45 deg.
   */
  float oversteer_slip_thresh_deg{20.0f};

  /**
   * Порог скорости изменения угла заноса для срабатывания [deg/s].
   * Диапазон: 10–500 dps.
   */
  float oversteer_rate_thresh_deg_s{50.0f};

  /**
   * Снижение газа при срабатывании oversteer: throttle *= (1 - reduction).
   * 0 = не снижать, 1 = полный стоп.
   * Диапазон: 0–1. Активно только в режимах normal/sport (не drift).
   */
  float oversteer_throttle_reduction{0.0f};

  /** Валидность конфигурации (magic number для проверки NVS) */
  uint32_t magic{0x53544142};  // 'STAB'

  /**
   * @brief Проверить валидность конфигурации
   * @return true если конфигурация валидна
   */
  [[nodiscard]] bool IsValid() const noexcept {
    return magic == 0x53544142 && madgwick_beta > 0.0f &&
           madgwick_beta <= 1.0f && lpf_cutoff_hz >= 5.0f &&
           lpf_cutoff_hz <= 100.0f && imu_sample_rate_hz > 0.0f &&
           pid_kp >= 0.0f && pid_ki >= 0.0f && pid_kd >= 0.0f &&
           pid_max_correction > 0.0f && steer_to_yaw_rate_dps > 0.0f;
  }

  /**
   * @brief Сбросить конфигурацию к значениям по умолчанию
   */
  void Reset() noexcept {
    enabled = false;
    madgwick_beta = 0.1f;
    lpf_cutoff_hz = 30.0f;
    imu_sample_rate_hz = 500.0f;
    mode = 0;
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
    magic = 0x53544142;
  }

  /**
   * @brief Применить предустановки PID для текущего режима (mode).
   *
   * Устанавливает pid_kp/ki/kd, pid_max_correction, steer_to_yaw_rate_dps
   * в зависимости от mode:
   *   0 = normal  — консервативный контроль рыскания
   *   1 = sport   — агрессивные параметры, высокая отзывчивость
   *   2 = drift   — мягкий контроль, допускает занос
   *
   * Не изменяет: enabled, madgwick_beta, lpf_cutoff_hz, fade_ms, magic.
   */
  void ApplyModeDefaults() noexcept {
    switch (mode) {
      case 1:  // Sport: быстрый отклик, лёгкий slip assist
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
      case 2:  // Drift: мягкая коррекция yaw, slip angle PID включён
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
      default:  // Normal (0): базовые параметры, slip PID выключен
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

  /**
   * @brief Применить ограничения к параметрам
   */
  void Clamp() noexcept {
    if (madgwick_beta < 0.01f) madgwick_beta = 0.01f;
    if (madgwick_beta > 1.0f) madgwick_beta = 1.0f;
    if (lpf_cutoff_hz < 5.0f) lpf_cutoff_hz = 5.0f;
    if (lpf_cutoff_hz > 100.0f) lpf_cutoff_hz = 100.0f;
    if (imu_sample_rate_hz < 100.0f) imu_sample_rate_hz = 100.0f;
    if (mode > 2) mode = 0;
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
    if (oversteer_rate_thresh_deg_s > 500.0f) oversteer_rate_thresh_deg_s = 500.0f;
    if (oversteer_throttle_reduction < 0.0f) oversteer_throttle_reduction = 0.0f;
    if (oversteer_throttle_reduction > 1.0f) oversteer_throttle_reduction = 1.0f;
  }
};

/**
 * @brief Конфигурация стабилизации по умолчанию
 */
namespace rc_vehicle::config {
struct StabilizationDefaults {
  static constexpr bool kEnabled = false;
  static constexpr float kMadgwickBeta = 0.1f;
  static constexpr float kLpfCutoffHz = 30.0f;
  static constexpr float kImuSampleRateHz = 500.0f;
  static constexpr uint8_t kMode = 0;
};
}  // namespace rc_vehicle::config