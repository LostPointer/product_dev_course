#include "vehicle_control_unified.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>

#include "config.hpp"
#include "control_loop_helpers.hpp"
#include "diagnostics_reporter.hpp"
#include "drive_mode_registry.hpp"
#include "log_format.hpp"
#include "rc_vehicle_common.hpp"
#include "telemetry_builder.hpp"

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
  if (!platform_) {
    return;  // Платформа не установлена
  }

  // Зарегистрировать control task в Task WDT
  platform_->RegisterTaskWdt();

  // Slew rate для плавности управления
  float commanded_throttle = 0.0f;
  float commanded_steering = 0.0f;
  float applied_throttle = 0.0f;
  float applied_steering = 0.0f;

  uint32_t last_pwm_update = platform_->GetTimeMs();
  uint32_t last_loop = platform_->GetTimeMs();

  // Диагностика
  uint32_t diag_loop_count = 0;
  uint32_t diag_start_ms = platform_->GetTimeMs();

  // Сигнализировать готовность control task (init-ready barrier)
  control_task_ready_.store(true, std::memory_order_release);

  while (true) {
    platform_->DelayUntilNextTick(config::ControlLoopConfig::kPeriodMs);
    const uint32_t now = platform_->GetTimeMs();
    const uint32_t dt_ms = now - last_loop;
    last_loop = now;
    ++diag_loop_count;

    // ─────────────────────────────────────────────────────────────────────
    // Обновление всех компонентов
    // ─────────────────────────────────────────────────────────────────────

    if (rc_handler_) rc_handler_->Update(now, dt_ms);
    if (wifi_handler_) wifi_handler_->Update(now, dt_ms);
    if (imu_handler_) imu_handler_->Update(now, dt_ms);

    // Sensor snapshot + CoM correction
    auto sensors = BuildSensorSnapshot(rc_handler_.get(), wifi_handler_.get(),
                                       imu_handler_.get());
    prev_gz_rad_s_ =
        CorrectImuForComOffset(sensors, imu_calib_, prev_gz_rad_s_, dt_ms);

    // ─────────────────────────────────────────────────────────────────────
    // EKF: оценка динамического состояния (vx, vy, r → slip angle)
    // ─────────────────────────────────────────────────────────────────────

    const bool ekf_active =
        stab_mgr_ && stab_mgr_->GetConfig().filter.ekf_enabled;
    if (ekf_active && sensors.imu_enabled && dt_ms > 0) {
      ekf_.UpdateFromImu(sensors.imu_data.ax, sensors.imu_data.ay,
                         sensors.imu_data.az, sensors.filtered_gz,
                         static_cast<float>(dt_ms) * 0.001f);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Обработка запроса калибровки
    // ─────────────────────────────────────────────────────────────────────

    calib_mgr_->ProcessRequest(now);
    calib_mgr_->ProcessCompletion();

    // ─────────────────────────────────────────────────────────────────────
    // Выбор источника управления (RC приоритетнее Wi-Fi)
    // ─────────────────────────────────────────────────────────────────────

    SelectControlSource(sensors, commanded_throttle, commanded_steering);

    // ─────────────────────────────────────────────────────────────────────
    // Авто-процедуры (forward calib, trim, test runner, CoM calib)
    // RC-пульт имеет приоритет (безопасность).
    // ─────────────────────────────────────────────────────────────────────

    {
      auto ad_out =
          auto_drive_.Update(BuildAutoDriveInput(sensors, imu_calib_, dt_ms));
      if (ad_out.active) {
        commanded_throttle = ad_out.throttle;
        commanded_steering = ad_out.steering;
      }

      HandleAutoDriveCompletion(ad_out, stab_mgr_.get(), imu_calib_,
                                *platform_);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Снимок конфигурации стабилизации (atomic snapshot — один вызов
    // GetConfig() за итерацию вместо нескольких)
    // ─────────────────────────────────────────────────────────────────────

    const auto stab_cfg = stab_mgr_ ? stab_mgr_->GetConfig()
                                     : StabilizationConfig{};

    // ─────────────────────────────────────────────────────────────────────
    // Плавное нарастание/спад веса стабилизации и переход между режимами
    // ─────────────────────────────────────────────────────────────────────

    stab_mgr_->UpdateWeights(dt_ms);

    const DriveMode drive_mode = stab_cfg.mode;
    const auto& strategy = DriveModeRegistry::Get(drive_mode);
    const auto traits = strategy.GetTraits();

    // ─────────────────────────────────────────────────────────────────────
    // Kids Mode: применить ограничения газа/руля и anti-spin
    // (активируется через traits.apply_input_limits)
    // ─────────────────────────────────────────────────────────────────────

    if (traits.apply_input_limits) {
      float kids_fwd_accel = 0.0f;
      if (sensors.imu_enabled) {
        kids_fwd_accel = imu_calib_.GetForwardAccel(sensors.imu_data);
      }
      kids_processor_.Process(commanded_throttle, commanded_steering, dt_ms,
                              kids_fwd_accel);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Стабилизационный pipeline (управляется ModeTraits)
    // ─────────────────────────────────────────────────────────────────────

    const float stab_weight = stab_mgr_->GetStabilizationWeight();
    const float mode_weight = stab_mgr_->GetModeTransitionWeight();

    if (traits.yaw_rate_active)
      yaw_ctrl_.Process(commanded_steering, stab_weight, mode_weight, dt_ms);

    if (traits.pitch_comp_active)
      pitch_ctrl_.Process(commanded_throttle, stab_weight);

    if (traits.slip_angle_active)
      slip_ctrl_.Process(commanded_throttle, stab_weight, mode_weight, dt_ms);

    if (traits.oversteer_guard_active)
      oversteer_guard_.Process(commanded_throttle, dt_ms,
                               traits.oversteer_reduces_throttle);

    // ─────────────────────────────────────────────────────────────────────
    // Failsafe
    // ─────────────────────────────────────────────────────────────────────

    if (platform_->FailsafeUpdate(sensors.rc_active, sensors.wifi_active)) {
      // Failsafe активен: нейтраль
      commanded_throttle = 0.0f;
      commanded_steering = 0.0f;
      applied_throttle = 0.0f;
      applied_steering = 0.0f;
      yaw_ctrl_.Reset();
      slip_ctrl_.Reset();
      oversteer_guard_.Reset();
      kids_processor_.Reset();
      ekf_.Reset();
      stab_mgr_->ResetWeights();       // Сброс весов стабилизации
      telem_mgr_->ResetLastLogTime();  // Сброс таймера лога
      auto_drive_.StopAll();           // Прервать все авто-процедуры
      platform_->SetPwmNeutral();
    }

    // ─────────────────────────────────────────────────────────────────────
    // Обновление PWM с slew rate
    // ─────────────────────────────────────────────────────────────────────

    // Применить trim (компенсация механического смещения нейтрали)
    const float steer_trim = stab_cfg.steering_trim;
    const float thr_trim = stab_cfg.throttle_trim;

    if (traits.use_slew_rate) {
      UpdatePwmWithSlewRate(*platform_, now, commanded_throttle,
                            commanded_steering, applied_throttle,
                            applied_steering, last_pwm_update, thr_trim,
                            steer_trim, stab_cfg.slew_throttle,
                            stab_cfg.slew_steering);
    } else {
      applied_throttle = commanded_throttle + thr_trim;
      applied_steering = commanded_steering + steer_trim;
      platform_->SetPwm(applied_throttle, applied_steering);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Телеметрия
    // ─────────────────────────────────────────────────────────────────────

    if (telem_handler_) {
      const TelemetryContext tctx{ekf_, madgwick_, imu_calib_,
                                  oversteer_guard_, kids_processor_,
                                  auto_drive_};
      auto snap = BuildTelemetrySnapshot(tctx, now, sensors, stab_cfg,
                                         drive_mode, applied_throttle,
                                         applied_steering, commanded_throttle,
                                         commanded_steering);
      telem_handler_->SendTelemetry(now, snap);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Запись в кольцевой буфер телеметрии (Phase 4.3) — 100 Hz
    // ─────────────────────────────────────────────────────────────────────

    if (sensors.imu_enabled) {
      const uint32_t last_log = telem_mgr_->GetLastLogTime();
      if (now - last_log >= config::TelemetryLogConfig::kLogIntervalMs) {
        const TelemetryContext tctx{ekf_, madgwick_, imu_calib_,
                                    oversteer_guard_, kids_processor_,
                                    auto_drive_};
        auto frame = BuildLogFrame(tctx, now, sensors, applied_throttle,
                                   applied_steering, commanded_throttle,
                                   commanded_steering);
        telem_mgr_->Push(frame);
        telem_mgr_->SetLastLogTime(now);

#ifdef ESP_PLATFORM
        // UDP telemetry streaming (no-op если не активен)
        UdpTelemEnqueue(frame);
#endif
      }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Диагностика
    // ─────────────────────────────────────────────────────────────────────

    {
      const DiagnosticsContext dctx{*platform_, *stab_mgr_, madgwick_, ekf_,
                                    imu_handler_.get(), last_loop_hz_};
      PrintDiagnostics(dctx, now, diag_loop_count, diag_start_ms);
    }

    // Кормить watchdog — если цикл зависнет, WDT перезагрузит устройство
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