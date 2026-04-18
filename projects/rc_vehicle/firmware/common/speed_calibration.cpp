#include "speed_calibration.hpp"

#include <algorithm>
#include <cmath>

namespace rc_vehicle {

bool SpeedCalibration::Start(float target_throttle, float cruise_duration_sec) {
  if (IsActive()) return false;
  if (target_throttle < 0.1f || target_throttle > 0.8f) return false;
  if (cruise_duration_sec < 1.0f || cruise_duration_sec > 10.0f) return false;

  target_throttle_ = target_throttle;
  cruise_duration_sec_ = cruise_duration_sec;

  MotionDriver::Config cfg;
  cfg.accel_mode = MotionDriver::AccelMode::LinearRamp;
  cfg.target_value = target_throttle_;
  cfg.accel_duration_sec = 1.5f;
  cfg.min_effective_throttle = 0.15f;
  cfg.brake_throttle = kBrakeThrottle;
  cfg.brake_timeout_sec = kBrakeTimeoutSec;
  cfg.zupt = {0.05f, 0.0f};  // gyro_thresh=0 → no gyro check
  driver_.Start(cfg);

  result_ = {};
  phase_elapsed_sec_ = 0.0f;
  speed_sum_ = 0.0;
  speed_count_ = 0;
  phase_ = Phase::Accelerate;
  return true;
}

void SpeedCalibration::Stop() {
  driver_.Reset();
  phase_ = Phase::Idle;
  result_ = {};
  phase_elapsed_sec_ = 0.0f;
  speed_sum_ = 0.0;
  speed_count_ = 0;
}

void SpeedCalibration::Update(float speed_ms, float accel_mag, float dt_sec,
                              float& throttle, float& steering) {
  steering = 0.0f;

  switch (phase_) {
    // ─────────────────────────────────────────────────────────────────────
    case Phase::Accelerate: {
      // MotionDriver handles linear ramp; pass dummy accel_g (not used in ramp)
      throttle = driver_.Update(0.0f, 2.0f, 0.0f, dt_sec);
      if (driver_.GetPhase() == MotionPhase::Cruise) {
        phase_ = Phase::Cruise;
        phase_elapsed_sec_ = 0.0f;
      }
      break;
    }

    // ─────────────────────────────────────────────────────────────────────
    case Phase::Cruise: {
      throttle = driver_.GetCruiseThrottle();
      phase_elapsed_sec_ += dt_sec;

      speed_sum_ += static_cast<double>(speed_ms);
      ++speed_count_;

      if (phase_elapsed_sec_ >= cruise_duration_sec_) {
        if (speed_count_ < kMinSamples) {
          phase_ = Phase::Failed;
        } else {
          phase_ = Phase::Brake;
          phase_elapsed_sec_ = 0.0f;
        }
      }
      break;
    }

    // ─────────────────────────────────────────────────────────────────────
    case Phase::Brake: {
      throttle = kBrakeThrottle;
      phase_elapsed_sec_ += dt_sec;

      // Остановка по ZUPT: малое ускорение → машина стоит
      if (accel_mag < kStopAccelThresh) {
        ComputeResult();
        phase_ = Phase::Done;
        throttle = 0.0f;
        break;
      }

      // Таймаут торможения
      if (phase_elapsed_sec_ >= kBrakeTimeoutSec) {
        ComputeResult();
        phase_ = Phase::Done;
        throttle = 0.0f;
      }
      break;
    }

    case Phase::Done:
    case Phase::Failed:
    case Phase::Idle:
      throttle = 0.0f;
      break;
  }
}

void SpeedCalibration::Reset() {
  driver_.Reset();
  result_ = {};
  phase_ = Phase::Idle;
  phase_elapsed_sec_ = 0.0f;
  speed_sum_ = 0.0;
  speed_count_ = 0;
}

void SpeedCalibration::ComputeResult() {
  result_.target_throttle = target_throttle_;
  result_.samples = speed_count_;

  if (speed_count_ < kMinSamples || target_throttle_ < 1e-4f) {
    result_.valid = false;
    return;
  }

  result_.mean_speed_ms =
      static_cast<float>(speed_sum_ / static_cast<double>(speed_count_));
  result_.speed_gain = result_.mean_speed_ms / target_throttle_;
  result_.valid = result_.mean_speed_ms > 0.01f;
}

}  // namespace rc_vehicle
