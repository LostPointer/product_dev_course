#include <gtest/gtest.h>

#include <cmath>

#include "imu_calibration.hpp"
#include "madgwick_filter.hpp"
#include "mock_platform.hpp"
#include "stabilization_pipeline.hpp"
#include "vehicle_ekf.hpp"

using rc_vehicle::DriveMode;
using rc_vehicle::ImuHandler;
using rc_vehicle::OversteerGuard;
using rc_vehicle::PitchCompensator;
using rc_vehicle::SlipAngleController;
using rc_vehicle::StabilizationConfig;
using rc_vehicle::VehicleEkf;
using rc_vehicle::YawRateController;
using rc_vehicle::testing::FakePlatform;

// ─── Вспомогательная функция ─────────────────────────────────────────────────

// Конвергируем MadgwickFilter к заданному pitch (градусы).
// ax/az рассчитываются из угла, gy = 0. Возвращает достигнутый pitch.
static float ConvergeMadgwickPitch(MadgwickFilter& mf, float target_deg,
                                   int iterations = 1000) {
  mf.SetBeta(0.9f);
  const float rad = target_deg * static_cast<float>(M_PI) / 180.0f;
  const float ax = std::sin(rad);
  const float az = std::cos(rad);
  for (int i = 0; i < iterations; ++i) {
    mf.Update(ax, 0.0f, az, 0.0f, 0.0f, 0.0f, 0.002f);
  }
  float pitch_deg = 0.0f, roll_deg = 0.0f, yaw_deg = 0.0f;
  mf.GetEulerDeg(pitch_deg, roll_deg, yaw_deg);
  return pitch_deg;
}

// ═════════════════════════════════════════════════════════════════════════════
// YawRateController
// ═════════════════════════════════════════════════════════════════════════════

class YawRateControllerTest : public ::testing::Test {
 protected:
  FakePlatform platform_;
  ImuCalibration calib_;
  MadgwickFilter madgwick_;
  ImuHandler imu_{platform_, calib_, madgwick_, 2};
  VehicleEkf ekf_;
  StabilizationConfig cfg_;
  YawRateController ctrl_;

  void SetUp() override {
    cfg_.Reset();
    cfg_.mode = DriveMode::Normal;
    cfg_.pid_kp = 0.1f;
    cfg_.pid_ki = 0.0f;
    cfg_.pid_kd = 0.0f;
    cfg_.pid_max_integral = 1.0f;
    cfg_.pid_max_correction = 0.3f;
    cfg_.steer_to_yaw_rate_dps = 90.0f;
    imu_.SetEnabled(true);
    ctrl_.Init(cfg_, ekf_, &imu_);
  }
};

// Нормальный режим: yaw PID корректирует руль (error = 90*0.5 - 0 = 45 dps)
TEST_F(YawRateControllerTest, NormalMode_CorrectsSteering) {
  float steering = 0.5f;
  ctrl_.Process(steering, /*stab_w=*/1.0f, /*mode_w=*/1.0f, /*dt_ms=*/2);
  EXPECT_GT(steering, 0.5f);  // Steering увеличился (PID добавил поправку)
  EXPECT_LE(steering, 1.0f);
}

// Sport режим: yaw PID активен (mode=Sport)
TEST_F(YawRateControllerTest, SportMode_CorrectsSteering) {
  cfg_.mode = DriveMode::Sport;
  ctrl_.Init(cfg_, ekf_, &imu_);
  float steering = 0.5f;
  ctrl_.Process(steering, 1.0f, 1.0f, 2);
  EXPECT_GT(steering, 0.5f);
}

// Drift режим: yaw PID не активен
TEST_F(YawRateControllerTest, DriftMode_NoChange) {
  cfg_.mode = DriveMode::Drift;
  ctrl_.Init(cfg_, ekf_, &imu_);
  float steering = 0.5f;
  ctrl_.Process(steering, 1.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(steering, 0.5f);
}

// stab_w=0: PID не применяется
TEST_F(YawRateControllerTest, StabWeightZero_NoChange) {
  float steering = 0.5f;
  ctrl_.Process(steering, /*stab_w=*/0.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(steering, 0.5f);
}

// IMU выключен: PID не применяется
TEST_F(YawRateControllerTest, ImuDisabled_NoChange) {
  imu_.SetEnabled(false);
  float steering = 0.5f;
  ctrl_.Process(steering, 1.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(steering, 0.5f);
}

// dt_ms=0: PID не применяется
TEST_F(YawRateControllerTest, ZeroDt_NoChange) {
  float steering = 0.5f;
  ctrl_.Process(steering, 1.0f, 1.0f, /*dt_ms=*/0);
  EXPECT_FLOAT_EQ(steering, 0.5f);
}

// mode_w масштабирует выход: при mode_w=0.5 поправка вдвое меньше
TEST_F(YawRateControllerTest, ModeWeightScalesOutput) {
  float s_full = 0.5f;
  float s_half = 0.5f;
  ctrl_.Process(s_full, 1.0f, /*mode_w=*/1.0f, 2);
  ctrl_.Reset();
  ctrl_.Process(s_half, 1.0f, /*mode_w=*/0.5f, 2);
  // s_full должен быть дальше от 0.5 чем s_half
  EXPECT_GT(s_full - 0.5f, s_half - 0.5f);
}

// Adaptive PID: при высокой скорости масштаб > 1
TEST_F(YawRateControllerTest, AdaptivePid_HighSpeed_IncreasesOutput) {
  cfg_.adaptive_pid_enabled = true;
  cfg_.adaptive_speed_ref_ms = 1.0f;
  cfg_.adaptive_scale_min = 0.5f;
  cfg_.adaptive_scale_max = 3.0f;
  ctrl_.Init(cfg_, ekf_, &imu_);

  // Без адаптивного
  float s_no_adapt = 0.5f;
  cfg_.adaptive_pid_enabled = false;
  ctrl_.Init(cfg_, ekf_, &imu_);
  ctrl_.Process(s_no_adapt, 1.0f, 1.0f, 2);

  // С адаптивным и высокой скоростью EKF (scale = 2/1 = 2, зажатый к max=3)
  cfg_.adaptive_pid_enabled = true;
  ctrl_.Init(cfg_, ekf_, &imu_);
  ekf_.SetState(/*vx=*/2.0f, /*vy=*/0.0f, /*r=*/0.0f);
  float s_adapt = 0.5f;
  ctrl_.Process(s_adapt, 1.0f, 1.0f, 2);

  EXPECT_GT(s_adapt - 0.5f, s_no_adapt - 0.5f);
}

// Adaptive PID: масштаб ограничен min при низкой скорости
TEST_F(YawRateControllerTest, AdaptivePid_LowSpeed_UsesScaleMin) {
  cfg_.adaptive_pid_enabled = true;
  cfg_.adaptive_speed_ref_ms = 5.0f;  // эталонная скорость 5 m/s
  cfg_.adaptive_scale_min = 0.1f;
  cfg_.adaptive_scale_max = 2.0f;
  ctrl_.Init(cfg_, ekf_, &imu_);

  // EKF speed ≈ 0 → scale = clamp(0/5, 0.1, 2.0) = 0.1
  float s_adapt = 0.5f;
  ctrl_.Process(s_adapt, 1.0f, 1.0f, 2);

  // Без адаптивного
  cfg_.adaptive_pid_enabled = false;
  ctrl_.Init(cfg_, ekf_, &imu_);
  float s_no_adapt = 0.5f;
  ctrl_.Process(s_no_adapt, 1.0f, 1.0f, 2);

  // При adaptive с низкой скоростью поправка меньше, чем без adaptive
  EXPECT_LT(s_adapt - 0.5f, s_no_adapt - 0.5f);
}

// Reset очищает интегратор
TEST_F(YawRateControllerTest, Reset_ClearsIntegral) {
  cfg_.pid_ki = 1.0f;
  ctrl_.Init(cfg_, ekf_, &imu_);

  // steering=0.5 → omega_desired=45 dps, omega_actual=0 → error=45 → integral ↑
  float steering = 0.5f;
  ctrl_.Process(steering, 1.0f, 1.0f, 2);
  EXPECT_GT(ctrl_.GetPid().GetIntegral(), 0.0f);

  ctrl_.Reset();
  EXPECT_FLOAT_EQ(ctrl_.GetPid().GetIntegral(), 0.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
// PitchCompensator
// ═════════════════════════════════════════════════════════════════════════════

class PitchCompensatorTest : public ::testing::Test {
 protected:
  FakePlatform platform_;
  ImuCalibration calib_;
  MadgwickFilter madgwick_;
  ImuHandler imu_{platform_, calib_, madgwick_, 2};
  StabilizationConfig cfg_;
  PitchCompensator ctrl_;

  void SetUp() override {
    cfg_.Reset();
    cfg_.pitch_comp_enabled = true;
    cfg_.pitch_comp_gain = 0.01f;
    cfg_.pitch_comp_max_correction = 0.25f;
    imu_.SetEnabled(true);
    ctrl_.Init(cfg_, madgwick_, &imu_);
  }
};

// pitch_comp_enabled=false: коррекция не применяется
TEST_F(PitchCompensatorTest, Disabled_NoChange) {
  cfg_.pitch_comp_enabled = false;
  ctrl_.Init(cfg_, madgwick_, &imu_);
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// stab_w=0: коррекция не применяется
TEST_F(PitchCompensatorTest, StabWeightZero_NoChange) {
  float throttle = 0.5f;
  ctrl_.Process(throttle, /*stab_w=*/0.0f);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// IMU выключен: коррекция не применяется
TEST_F(PitchCompensatorTest, ImuDisabled_NoChange) {
  imu_.SetEnabled(false);
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// Нулевой pitch (дефолтный Madgwick q=(1,0,0,0)): нет коррекции
TEST_F(PitchCompensatorTest, ZeroPitch_NoChange) {
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// Положительный pitch → газ увеличивается
TEST_F(PitchCompensatorTest, PositivePitch_IncreasesThrottle) {
  ConvergeMadgwickPitch(madgwick_, /*target_deg=*/5.0f);

  float pitch_deg = 0.0f, roll_deg = 0.0f, yaw_deg = 0.0f;
  madgwick_.GetEulerDeg(pitch_deg, roll_deg, yaw_deg);
  // Проверяем, что фильтр сошёлся к ненулевому pitch
  ASSERT_NE(pitch_deg, 0.0f) << "Madgwick не сошёлся к ненулевому pitch";

  float throttle = 0.5f;
  const float throttle_before = throttle;
  ctrl_.Process(throttle, 1.0f);

  if (pitch_deg > 0.0f) {
    EXPECT_GT(throttle, throttle_before);
  } else {
    EXPECT_LT(throttle, throttle_before);
  }
}

// Большой gain → коррекция ограничена max_correction
TEST_F(PitchCompensatorTest, LargeGain_ClampsCorrection) {
  // Достигаем ненулевого pitch
  float actual_pitch = ConvergeMadgwickPitch(madgwick_, /*target_deg=*/5.0f);
  ASSERT_NE(actual_pitch, 0.0f);

  // Огромный gain → коррекция = gain * pitch >> max_correction
  cfg_.pitch_comp_gain = 1.0f;   // 1.0 * 5 deg = 5 >> max_correction=0.1
  cfg_.pitch_comp_max_correction = 0.1f;
  ctrl_.Init(cfg_, madgwick_, &imu_);

  float throttle = 0.0f;
  ctrl_.Process(throttle, 1.0f);

  // Коррекция не должна превысить ±max_correction
  EXPECT_LE(std::abs(throttle), 0.1f + 1e-5f);
}

// std::clamp симметричен: отрицательный pitch → отрицательная коррекция
TEST_F(PitchCompensatorTest, NegativePitch_SymmetricClamp) {
  // В конвенции Madgwick ax=sin(+5°) соответствует nose-down (отрицательный pitch)
  float actual_pitch = ConvergeMadgwickPitch(madgwick_, /*target_deg=*/5.0f);
  ASSERT_NE(actual_pitch, 0.0f);

  cfg_.pitch_comp_gain = 1.0f;
  cfg_.pitch_comp_max_correction = 0.1f;
  ctrl_.Init(cfg_, madgwick_, &imu_);

  float throttle = 0.0f;
  ctrl_.Process(throttle, 1.0f);

  // Отрицательная коррекция не должна превышать -max_correction
  EXPECT_GE(throttle, -0.1f - 1e-5f);
  EXPECT_LE(throttle, 0.0f + 1e-5f);
}

// ═════════════════════════════════════════════════════════════════════════════
// SlipAngleController
// ═════════════════════════════════════════════════════════════════════════════

class SlipAngleControllerTest : public ::testing::Test {
 protected:
  FakePlatform platform_;
  ImuCalibration calib_;
  MadgwickFilter madgwick_;
  ImuHandler imu_{platform_, calib_, madgwick_, 2};
  VehicleEkf ekf_;
  StabilizationConfig cfg_;
  SlipAngleController ctrl_;

  void SetUp() override {
    cfg_.Reset();
    cfg_.mode = DriveMode::Drift;
    cfg_.slip_target_deg = 15.0f;
    cfg_.slip_kp = 0.01f;
    cfg_.slip_ki = 0.0f;
    cfg_.slip_kd = 0.0f;
    cfg_.slip_max_integral = 5.0f;
    cfg_.slip_max_correction = 0.25f;
    imu_.SetEnabled(true);
    // Установить состояние EKF для ненулевого slip angle
    // slip = atan2(vy, vx) → задаём vx=1, vy=0 → slip=0 (error = 15-0 = 15 deg)
    ekf_.SetState(1.0f, 0.0f, 0.0f);
    ctrl_.Init(cfg_, ekf_, &imu_);
  }
};

// Normal mode: slip PID не активен
TEST_F(SlipAngleControllerTest, NormalMode_NoChange) {
  cfg_.mode = DriveMode::Normal;
  ctrl_.Init(cfg_, ekf_, &imu_);
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// Sport mode: slip PID не активен
TEST_F(SlipAngleControllerTest, SportMode_NoChange) {
  cfg_.mode = DriveMode::Sport;
  ctrl_.Init(cfg_, ekf_, &imu_);
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// Drift mode + ненулевая ошибка: slip PID корректирует газ
TEST_F(SlipAngleControllerTest, DriftMode_CorrectionsThrottle) {
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f, 1.0f, 2);
  // slip_error = 15 - 0 = 15, kp=0.01 → output = 0.15 → clamp(0.65, -1, 1) = 0.65
  EXPECT_GT(throttle, 0.5f);
}

// stab_w=0: slip PID не применяется
TEST_F(SlipAngleControllerTest, StabWeightZero_NoChange) {
  float throttle = 0.5f;
  ctrl_.Process(throttle, /*stab_w=*/0.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// IMU выключен: slip PID не применяется
TEST_F(SlipAngleControllerTest, ImuDisabled_NoChange) {
  imu_.SetEnabled(false);
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f, 1.0f, 2);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// dt_ms=0: slip PID не применяется
TEST_F(SlipAngleControllerTest, ZeroDt_NoChange) {
  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f, 1.0f, /*dt_ms=*/0);
  EXPECT_FLOAT_EQ(throttle, 0.5f);
}

// mode_w масштабирует выход slip PID
TEST_F(SlipAngleControllerTest, ModeWeightScalesOutput) {
  float t_full = 0.5f;
  float t_half = 0.5f;
  ctrl_.Process(t_full, 1.0f, /*mode_w=*/1.0f, 2);
  ctrl_.Reset();
  ctrl_.Process(t_half, 1.0f, /*mode_w=*/0.5f, 2);
  EXPECT_GT(t_full - 0.5f, t_half - 0.5f);
}

// Reset очищает интегратор
TEST_F(SlipAngleControllerTest, Reset_ClearsIntegral) {
  cfg_.slip_ki = 1.0f;
  ctrl_.Init(cfg_, ekf_, &imu_);

  float throttle = 0.5f;
  ctrl_.Process(throttle, 1.0f, 1.0f, 2);
  EXPECT_GT(ctrl_.GetPid().GetIntegral(), 0.0f);

  ctrl_.Reset();
  EXPECT_FLOAT_EQ(ctrl_.GetPid().GetIntegral(), 0.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
// OversteerGuard
// ═════════════════════════════════════════════════════════════════════════════

class OversteerGuardTest : public ::testing::Test {
 protected:
  FakePlatform platform_;
  ImuCalibration calib_;
  MadgwickFilter madgwick_;
  ImuHandler imu_{platform_, calib_, madgwick_, 2};
  VehicleEkf ekf_;
  StabilizationConfig cfg_;
  OversteerGuard guard_;

  void SetUp() override {
    cfg_.Reset();
    cfg_.oversteer_warn_enabled = true;
    cfg_.oversteer_slip_thresh_deg = 20.0f;
    cfg_.oversteer_rate_thresh_deg_s = 50.0f;
    cfg_.oversteer_throttle_reduction = 0.3f;
    cfg_.mode = DriveMode::Normal;
    imu_.SetEnabled(true);
    guard_.Init(cfg_, ekf_, &imu_);
  }
};

// Ниже порогов: not active, газ не меняется
TEST_F(OversteerGuardTest, BelowThreshold_NotActive) {
  ekf_.SetState(1.0f, 0.0f, 0.0f);  // slip ≈ 0°
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  EXPECT_FALSE(guard_.IsActive());
  EXPECT_FLOAT_EQ(throttle, 0.8f);
}

// |slip| > thresh и |rate| > thresh → активен
TEST_F(OversteerGuardTest, AboveThreshold_IsActive) {
  // Установить начальный slip
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);  // slip ≈ 25°
  // Первый шаг: prev_slip=0, slip=25°, rate=(25-0)/0.002=12500 dps >> thresh
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  EXPECT_TRUE(guard_.IsActive());
}

// В normal mode: активный oversteer снижает газ
TEST_F(OversteerGuardTest, NormalMode_ReducesThrottle) {
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  EXPECT_TRUE(guard_.IsActive());
  EXPECT_LT(throttle, 0.8f);
  // Снижение: throttle *= (1 - 0.3) = 0.8 * 0.7 = 0.56
  EXPECT_NEAR(throttle, 0.8f * (1.0f - 0.3f), 1e-5f);
}

// В Drift mode: oversteer не снижает газ (занос ожидаемый)
TEST_F(OversteerGuardTest, DriftMode_NoThrottleReduction) {
  cfg_.mode = DriveMode::Drift;
  guard_.Init(cfg_, ekf_, &imu_);
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  // Флаг может сработать, но газ не снижается
  EXPECT_FLOAT_EQ(throttle, 0.8f);
}

// oversteer_warn_enabled=false: не срабатывает
TEST_F(OversteerGuardTest, Disabled_NotActive) {
  cfg_.oversteer_warn_enabled = false;
  guard_.Init(cfg_, ekf_, &imu_);
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  EXPECT_FALSE(guard_.IsActive());
  EXPECT_FLOAT_EQ(throttle, 0.8f);
}

// IMU выключен: guard не работает
TEST_F(OversteerGuardTest, ImuDisabled_NotActive) {
  imu_.SetEnabled(false);
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  EXPECT_FALSE(guard_.IsActive());
  EXPECT_FLOAT_EQ(throttle, 0.8f);
}

// dt_ms=0: guard не работает
TEST_F(OversteerGuardTest, ZeroDt_NotActive) {
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, /*dt_ms=*/0);
  EXPECT_FALSE(guard_.IsActive());
  EXPECT_FLOAT_EQ(throttle, 0.8f);
}

// Reset: очищает флаг и prev_slip
TEST_F(OversteerGuardTest, Reset_ClearsState) {
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  EXPECT_TRUE(guard_.IsActive());

  guard_.Reset();
  EXPECT_FALSE(guard_.IsActive());

  // После Reset prev_slip=0, следующий вызов с тем же slip → большой rate снова
  float throttle2 = 0.8f;
  guard_.Process(throttle2, 2);
  // Поскольку prev_slip был сброшен, rate опять большой → активен снова
  EXPECT_TRUE(guard_.IsActive());
}

// GetActivePtr возвращает указатель на тот же bool что IsActive
TEST_F(OversteerGuardTest, GetActivePtr_PointsToIsActive) {
  const bool* ptr = guard_.GetActivePtr();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(*ptr, guard_.IsActive());

  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);

  EXPECT_EQ(*ptr, guard_.IsActive());
  EXPECT_TRUE(*ptr);
}

// oversteer_throttle_reduction=0: флаг срабатывает, но газ не меняется
TEST_F(OversteerGuardTest, ZeroReduction_FlagActivButNoThrottleCut) {
  cfg_.oversteer_throttle_reduction = 0.0f;
  guard_.Init(cfg_, ekf_, &imu_);
  ekf_.SetState(1.0f, std::tan(25.0f * static_cast<float>(M_PI) / 180.0f),
                0.0f);
  float throttle = 0.8f;
  guard_.Process(throttle, 2);
  // Флаг может сработать, но газ не изменяется (reduction=0)
  EXPECT_FLOAT_EQ(throttle, 0.8f);
}
