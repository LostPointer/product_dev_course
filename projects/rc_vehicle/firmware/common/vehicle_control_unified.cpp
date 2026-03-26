#include "vehicle_control_unified.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>

#include "config.hpp"
#include "control_loop_helpers.hpp"
#include "control_loop_processor.hpp"
#include "log_format.hpp"
#include "rc_vehicle_common.hpp"

#ifdef ESP_PLATFORM
#include "udp_telem_sender.hpp"
#endif

namespace rc_vehicle {

// ═════════════════════════════════════════════════════════════════════════
// VehicleControlUnified Implementation
// ═════════════════════════════════════════════════════════════════════════

void VehicleControlUnified::SetPlatform(
    std::unique_ptr<VehicleControlPlatform> platform) {
  platform_ = std::move(platform);
}

void VehicleControlUnified::ControlTaskEntry(void* arg) {
  auto* self = static_cast<VehicleControlUnified*>(arg);
  if (self) {
    self->ControlTaskLoop();
  }
}

void VehicleControlUnified::ControlTaskLoop() {
  if (!platform_) return;
  platform_->RegisterTaskWdt();

  const ControlLoopContext ctx{
      *platform_,       imu_calib_,        madgwick_,    ekf_,
      yaw_ctrl_,        pitch_ctrl_,        slip_ctrl_,   oversteer_guard_,
      kids_processor_,  auto_drive_,
      calib_mgr_.get(), stab_mgr_.get(),    telem_mgr_.get(),
      rc_handler_.get(), wifi_handler_.get(), imu_handler_.get(),
      telem_handler_.get(), last_loop_hz_};

  const uint32_t start = platform_->GetTimeMs();
  ControlLoopProcessor processor(ctx, start);

  control_task_ready_.store(true, std::memory_order_release);

  uint32_t last_loop = start;
  while (true) {
    platform_->DelayUntilNextTick(config::ControlLoopConfig::kPeriodMs);
    const uint32_t now = platform_->GetTimeMs();
    processor.Step(now, now - last_loop);
    last_loop = now;
    platform_->FeedTaskWdt();
  }
}

PlatformError VehicleControlUnified::Init() {
  if (inited_) {
    return PlatformError::Ok;
  }

  if (!platform_) {
    return PlatformError::TaskCreateFailed;  // Платформа не установлена
  }

  // ───────────────────────────────────────────────────────────────────────
  // Инициализация платформы
  // ───────────────────────────────────────────────────────────────────────

  auto pwm_result = platform_->InitPwm();
  if (IsError(pwm_result)) {
    platform_->Log(LogLevel::Error, "Failed to initialize PWM");
    return GetError(pwm_result);
  }

  auto failsafe_result = platform_->InitFailsafe();
  if (IsError(failsafe_result)) {
    platform_->Log(LogLevel::Error, "Failed to initialize failsafe");
    return GetError(failsafe_result);
  }

  // RC input (опционально)
  auto rc_result = platform_->InitRc();
  if (IsOk(rc_result)) {
    rc_enabled_ = true;
  } else {
    rc_enabled_ = false;
    platform_->Log(LogLevel::Warning,
                   "RC input init failed — continuing without RC-in");
  }

  // IMU (опционально)
  InitImuSubsystem();

  // Инициализация кольцевого буфера телеметрии
  InitTelemetryLog();

  // ───────────────────────────────────────────────────────────────────────
  // Создание компонентов control loop
  // ───────────────────────────────────────────────────────────────────────

  if (!InitializeComponents()) {
    return PlatformError::TaskCreateFailed;
  }

  // ───────────────────────────────────────────────────────────────────────
  // Запуск control loop
  // ───────────────────────────────────────────────────────────────────────

  auto task_result = platform_->CreateTask(ControlTaskEntry, this);
  if (IsError(task_result)) {
    platform_->Log(LogLevel::Error, "Failed to create vehicle control task");
    return GetError(task_result);
  }

  inited_ = true;
  platform_->Log(LogLevel::Info,
                 "Vehicle control started (unified architecture)");
  return PlatformError::Ok;
}

bool VehicleControlUnified::InitializeComponents() {
  if (rc_enabled_) {
    rc_handler_.reset(
        new RcInputHandler(*platform_, config::RcInputConfig::kPollIntervalMs));
  }

  wifi_handler_.reset(new WifiCommandHandler(
      *platform_, config::WifiConfig::kCommandTimeoutMs));

  if (imu_enabled_) {
    imu_handler_.reset(new ImuHandler(*platform_, imu_calib_, madgwick_,
                                      config::ImuConfig::kReadIntervalMs));
    imu_handler_->SetEnabled(true);
    // Применить LPF cutoff из конфигурации
    imu_handler_->SetLpfCutoff(stab_mgr_->GetConfig().filter.lpf_cutoff_hz);
    // Обновить указатель на imu_handler в stab_mgr
    stab_mgr_.reset(new StabilizationManager(*platform_, madgwick_, yaw_ctrl_,
                                             slip_ctrl_, imu_handler_.get()));
    stab_mgr_->LoadFromNvs();
    stab_mgr_->ApplyConfig();
  }

  // Создаём пустые handlers если они не были созданы (для телеметрии)
  if (!rc_handler_) {
    rc_handler_.reset(new RcInputHandler(*platform_, 0));
  }
  if (!imu_handler_) {
    imu_handler_.reset(new ImuHandler(*platform_, imu_calib_, madgwick_, 0));
  }

  // Инициализация стратегий стабилизации
  const auto& cfg = stab_mgr_->GetConfig();
  yaw_ctrl_.Init(cfg, ekf_, imu_handler_.get());
  pitch_ctrl_.Init(cfg, madgwick_, imu_handler_.get());
  slip_ctrl_.Init(cfg, ekf_, imu_handler_.get());
  oversteer_guard_.Init(cfg, ekf_, imu_handler_.get());
  kids_processor_.Init(cfg, ekf_, imu_handler_.get());

  // Телеметрия
  telem_handler_.reset(new TelemetryHandler(
      *platform_, config::TelemetryConfig::kSendIntervalMs));

  return true;
}



bool VehicleControlUnified::StartComOffsetCalibration(
    float target_accel_g, float steering_magnitude,
    float cruise_duration_sec) {
  if (!stab_mgr_ || !imu_enabled_) return false;
  const auto& calib_data = imu_calib_.GetData();
  return auto_drive_.StartComCalib(target_accel_g, steering_magnitude,
                                   cruise_duration_sec,
                                   calib_data.gravity_vec);
}

bool VehicleControlUnified::StartTest(const TestParams& params) {
  if (!stab_mgr_ || !imu_enabled_) return false;
  return auto_drive_.StartTest(params);
}

bool VehicleControlUnified::StartSteeringTrimCalibration(
    float target_accel_g) {
  if (!stab_mgr_ || !imu_enabled_) return false;
  const auto& cfg = stab_mgr_->GetConfig();
  return auto_drive_.StartTrimCalib(target_accel_g, cfg.steering_trim,
                                    cfg.yaw_rate.steer_to_yaw_rate_dps);
}

void VehicleControlUnified::OnWifiCommand(float throttle, float steering) {
  if (platform_) {
    platform_->SendWifiCommand(throttle, steering);
  }
}

void VehicleControlUnified::InitImuSubsystem() {
  auto imu_result = platform_->InitImu();
  if (IsOk(imu_result)) {
    imu_enabled_ = true;

    calib_mgr_.reset(
        new CalibrationManager(*platform_, imu_calib_, madgwick_, &ekf_));
    stab_mgr_.reset(new StabilizationManager(*platform_, madgwick_, yaw_ctrl_,
                                             slip_ctrl_, nullptr));
    telem_mgr_.reset(new TelemetryManager());

    auto_drive_.SetCalibrationManager(calib_mgr_.get());
    calib_mgr_->LoadFromNvs();

    // Загрузка CoM offset из NVS
    float com_off[2]{0.f, 0.f};
    if (platform_->LoadComOffset(com_off)) {
      auto data = imu_calib_.GetData();
      data.com_offset[0] = com_off[0];
      data.com_offset[1] = com_off[1];
      imu_calib_.SetData(data);
    }

    stab_mgr_->LoadFromNvs();
    stab_mgr_->ApplyConfig();
    calib_mgr_->StartAutoCalibration();
  } else {
    imu_enabled_ = false;
    const int who = platform_->GetImuLastWhoAmI();
    platform_->Log(LogLevel::Warning,
                   "IMU init failed — continuing without IMU");
    if (who >= 0) {
      LogFormat fmt;
      fmt << "IMU WHO_AM_I = 0x" << std::hex << std::setw(2)
          << std::setfill('0') << static_cast<unsigned>(who);
      platform_->Log(LogLevel::Info, fmt.str());
    }
  }
}

void VehicleControlUnified::InitTelemetryLog() {
  if (!telem_mgr_->Init(config::TelemetryLogConfig::kCapacityFrames)) {
    platform_->Log(
        LogLevel::Warning,
        "TelemetryLog: failed to allocate (no PSRAM?), log disabled");
  } else {
    LogFormat fmt;
    fmt << "TelemetryLog: allocated "
        << static_cast<unsigned>(config::TelemetryLogConfig::kCapacityFrames)
        << " frames";
    platform_->Log(LogLevel::Info, fmt.str());
  }
}

std::vector<SelfTestItem> VehicleControlUnified::RunSelfTest() const {
  const SelfTestContext ctx{last_loop_hz_,   imu_handler_.get(),
                            madgwick_,       ekf_,
                            rc_handler_.get(), wifi_handler_.get(),
                            imu_calib_,      telem_mgr_.get(),
                            platform_ != nullptr, inited_};
  return SelfTest::Run(BuildSelfTestInput(ctx));
}

}  // namespace rc_vehicle