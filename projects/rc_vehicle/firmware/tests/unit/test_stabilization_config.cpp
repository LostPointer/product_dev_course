#include <gtest/gtest.h>

#include "stabilization_config.hpp"

using namespace rc_vehicle;

// ═══════════════════════════════════════════════════════════════════════════
// Default values
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, DefaultsAreValid) {
  StabilizationConfig cfg{};
  EXPECT_TRUE(cfg.IsValid());
}

TEST(StabilizationConfigTest, DefaultEnabledIsFalse) {
  StabilizationConfig cfg{};
  EXPECT_FALSE(cfg.enabled);
}

TEST(StabilizationConfigTest, DefaultModeIsNormal) {
  StabilizationConfig cfg{};
  EXPECT_EQ(cfg.mode, DriveMode::Normal);
}

TEST(StabilizationConfigTest, DefaultMadgwickBeta) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.madgwick_beta, 0.1f);
}

TEST(StabilizationConfigTest, DefaultLpfCutoffHz) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.lpf_cutoff_hz, 30.0f);
}

TEST(StabilizationConfigTest, DefaultPidGains) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.pid_kp, 0.1f);
  EXPECT_FLOAT_EQ(cfg.pid_ki, 0.0f);
  EXPECT_FLOAT_EQ(cfg.pid_kd, 0.005f);
}

TEST(StabilizationConfigTest, MagicNumberIsCorrect) {
  StabilizationConfig cfg{};
  EXPECT_EQ(cfg.magic, kStabilizationConfigMagic);
}

// ═══════════════════════════════════════════════════════════════════════════
// IsValid
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, IsValid_ZeroBeta_Invalid) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = 0.0f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_NegativeBeta_Invalid) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = -0.1f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_BetaAboveOne_Invalid) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = 1.01f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_BetaAtOne_Valid) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = 1.0f;
  EXPECT_TRUE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_LpfTooLow_Invalid) {
  StabilizationConfig cfg{};
  cfg.lpf_cutoff_hz = 4.9f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_LpfTooHigh_Invalid) {
  StabilizationConfig cfg{};
  cfg.lpf_cutoff_hz = 100.1f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_WrongMagic_Invalid) {
  StabilizationConfig cfg{};
  cfg.magic = 0xDEADBEEF;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_ZeroImuSampleRate_Invalid) {
  StabilizationConfig cfg{};
  cfg.imu_sample_rate_hz = 0.0f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_NegativeKp_Invalid) {
  StabilizationConfig cfg{};
  cfg.pid_kp = -0.01f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_ZeroMaxCorrection_Invalid) {
  StabilizationConfig cfg{};
  cfg.pid_max_correction = 0.0f;
  EXPECT_FALSE(cfg.IsValid());
}

TEST(StabilizationConfigTest, IsValid_ZeroSteerDps_Invalid) {
  StabilizationConfig cfg{};
  cfg.steer_to_yaw_rate_dps = 0.0f;
  EXPECT_FALSE(cfg.IsValid());
}

// ═══════════════════════════════════════════════════════════════════════════
// Reset
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, ResetRestoresDefaults) {
  StabilizationConfig cfg{};
  cfg.enabled = true;
  cfg.madgwick_beta = 0.9f;
  cfg.lpf_cutoff_hz = 80.0f;
  cfg.pid_kp = 5.0f;
  cfg.magic = 0xDEAD;

  cfg.Reset();

  EXPECT_FALSE(cfg.enabled);
  EXPECT_FLOAT_EQ(cfg.madgwick_beta, 0.1f);
  EXPECT_FLOAT_EQ(cfg.lpf_cutoff_hz, 30.0f);
  EXPECT_FLOAT_EQ(cfg.pid_kp, 0.1f);
  EXPECT_EQ(cfg.magic, kStabilizationConfigMagic);
  EXPECT_TRUE(cfg.IsValid());
}

// ═══════════════════════════════════════════════════════════════════════════
// Clamp
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, Clamp_BetaTooLow_ClampedTo001) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = 0.001f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.madgwick_beta, 0.01f);
}

TEST(StabilizationConfigTest, Clamp_BetaTooHigh_ClampedToOne) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = 5.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.madgwick_beta, 1.0f);
}

TEST(StabilizationConfigTest, Clamp_LpfTooLow_ClampedTo5Hz) {
  StabilizationConfig cfg{};
  cfg.lpf_cutoff_hz = 1.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.lpf_cutoff_hz, 5.0f);
}

TEST(StabilizationConfigTest, Clamp_LpfTooHigh_ClampedTo100Hz) {
  StabilizationConfig cfg{};
  cfg.lpf_cutoff_hz = 500.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.lpf_cutoff_hz, 100.0f);
}

TEST(StabilizationConfigTest, Clamp_MaxCorrectionAboveOne_ClampedToOne) {
  StabilizationConfig cfg{};
  cfg.pid_max_correction = 2.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.pid_max_correction, 1.0f);
}

TEST(StabilizationConfigTest, Clamp_MaxCorrectionNegative_ClampedToZero) {
  StabilizationConfig cfg{};
  cfg.pid_max_correction = -0.5f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.pid_max_correction, 0.0f);
}

TEST(StabilizationConfigTest, Clamp_SteerDpsTooLow_ClampedTo10) {
  StabilizationConfig cfg{};
  cfg.steer_to_yaw_rate_dps = 1.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 10.0f);
}

TEST(StabilizationConfigTest, Clamp_SteerDpsTooHigh_ClampedTo360) {
  StabilizationConfig cfg{};
  cfg.steer_to_yaw_rate_dps = 1000.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 360.0f);
}

TEST(StabilizationConfigTest, Clamp_FadeMsTooHigh_ClampedTo5000) {
  StabilizationConfig cfg{};
  cfg.fade_ms = 10000;
  cfg.Clamp();
  EXPECT_EQ(cfg.fade_ms, 5000u);
}

TEST(StabilizationConfigTest, Clamp_ModeTooHigh_ClampedToNormal) {
  StabilizationConfig cfg{};
  cfg.mode = static_cast<DriveMode>(5);
  cfg.Clamp();
  EXPECT_EQ(cfg.mode, DriveMode::Normal);
}

TEST(StabilizationConfigTest, Clamp_ValidValues_Unchanged) {
  StabilizationConfig cfg{};
  cfg.madgwick_beta = 0.5f;
  cfg.lpf_cutoff_hz = 50.0f;
  cfg.pid_max_correction = 0.3f;
  cfg.steer_to_yaw_rate_dps = 90.0f;
  cfg.mode = DriveMode::Sport;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.madgwick_beta, 0.5f);
  EXPECT_FLOAT_EQ(cfg.lpf_cutoff_hz, 50.0f);
  EXPECT_FLOAT_EQ(cfg.pid_max_correction, 0.3f);
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 90.0f);
  EXPECT_EQ(cfg.mode, DriveMode::Sport);
}

// ═══════════════════════════════════════════════════════════════════════════
// ApplyModeDefaults
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, ApplyModeDefaults_Normal_SetsNormalGains) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Normal;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pid_kp, 0.10f);
  EXPECT_FLOAT_EQ(cfg.pid_ki, 0.00f);
  EXPECT_FLOAT_EQ(cfg.pid_kd, 0.005f);
  EXPECT_FLOAT_EQ(cfg.pid_max_correction, 0.30f);
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 90.0f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Sport_SetsSportGains) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Sport;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pid_kp, 0.20f);
  EXPECT_FLOAT_EQ(cfg.pid_ki, 0.01f);
  EXPECT_FLOAT_EQ(cfg.pid_kd, 0.010f);
  EXPECT_FLOAT_EQ(cfg.pid_max_correction, 0.40f);
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 120.0f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Drift_SetsDriftGains) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Drift;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pid_kp, 0.05f);
  EXPECT_FLOAT_EQ(cfg.pid_ki, 0.00f);
  EXPECT_FLOAT_EQ(cfg.pid_kd, 0.002f);
  EXPECT_FLOAT_EQ(cfg.pid_max_correction, 0.20f);
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 60.0f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_UnknownMode_FallsToNormal) {
  StabilizationConfig cfg{};
  cfg.mode = static_cast<DriveMode>(99);
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pid_kp, 0.10f);
  EXPECT_FLOAT_EQ(cfg.steer_to_yaw_rate_dps, 90.0f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_DoesNotChangeOtherFields) {
  StabilizationConfig cfg{};
  const bool enabled = true;
  const float beta = 0.3f;
  const float cutoff = 25.0f;
  const uint32_t fade = 300;

  cfg.enabled = enabled;
  cfg.madgwick_beta = beta;
  cfg.lpf_cutoff_hz = cutoff;
  cfg.fade_ms = fade;
  cfg.mode = DriveMode::Sport;
  cfg.ApplyModeDefaults();

  EXPECT_EQ(cfg.enabled, enabled);
  EXPECT_FLOAT_EQ(cfg.madgwick_beta, beta);
  EXPECT_FLOAT_EQ(cfg.lpf_cutoff_hz, cutoff);
  EXPECT_EQ(cfg.fade_ms, fade);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_ResultIsValid) {
  for (auto mode : {DriveMode::Normal, DriveMode::Sport, DriveMode::Drift}) {
    StabilizationConfig cfg{};
    cfg.mode = mode;
    cfg.ApplyModeDefaults();
    EXPECT_TRUE(cfg.IsValid()) << "Mode " << static_cast<int>(mode);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Sport gains are more aggressive than normal
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, SportGainsAreStrongerThanNormal) {
  StabilizationConfig normal{};
  normal.mode = DriveMode::Normal;
  normal.ApplyModeDefaults();

  StabilizationConfig sport{};
  sport.mode = DriveMode::Sport;
  sport.ApplyModeDefaults();

  EXPECT_GT(sport.pid_kp, normal.pid_kp);
  EXPECT_GT(sport.pid_max_correction, normal.pid_max_correction);
  EXPECT_GT(sport.steer_to_yaw_rate_dps, normal.steer_to_yaw_rate_dps);
}

TEST(StabilizationConfigTest, DriftGainsAreSofterThanNormal) {
  StabilizationConfig normal{};
  normal.mode = DriveMode::Normal;
  normal.ApplyModeDefaults();

  StabilizationConfig drift{};
  drift.mode = DriveMode::Drift;
  drift.ApplyModeDefaults();

  EXPECT_LT(drift.pid_kp, normal.pid_kp);
  EXPECT_LT(drift.pid_max_correction, normal.pid_max_correction);
  EXPECT_LT(drift.steer_to_yaw_rate_dps, normal.steer_to_yaw_rate_dps);
}

// ═══════════════════════════════════════════════════════════════════════════
// Pitch compensation (slope stabilization)
// ═══════════════════════════════════════════════════════════════════════════

TEST(StabilizationConfigTest, DefaultPitchCompDisabled) {
  StabilizationConfig cfg{};
  EXPECT_FALSE(cfg.pitch_comp_enabled);
}

TEST(StabilizationConfigTest, DefaultPitchCompGain) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.01f);
}

TEST(StabilizationConfigTest, DefaultPitchCompMaxCorrection) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.25f);
}

TEST(StabilizationConfigTest, ResetRestoresPitchCompDefaults) {
  StabilizationConfig cfg{};
  cfg.pitch_comp_enabled = true;
  cfg.pitch_comp_gain = 0.05f;
  cfg.pitch_comp_max_correction = 0.5f;

  cfg.Reset();

  EXPECT_FALSE(cfg.pitch_comp_enabled);
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.01f);
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.25f);
}

TEST(StabilizationConfigTest, Clamp_PitchGainNegative_ClampedToZero) {
  StabilizationConfig cfg{};
  cfg.pitch_comp_gain = -0.01f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.0f);
}

TEST(StabilizationConfigTest, Clamp_PitchGainTooHigh_ClampedTo005) {
  StabilizationConfig cfg{};
  cfg.pitch_comp_gain = 0.1f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.05f);
}

TEST(StabilizationConfigTest, Clamp_PitchMaxCorrNegative_ClampedToZero) {
  StabilizationConfig cfg{};
  cfg.pitch_comp_max_correction = -0.1f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.0f);
}

TEST(StabilizationConfigTest, Clamp_PitchMaxCorrTooHigh_ClampedToHalf) {
  StabilizationConfig cfg{};
  cfg.pitch_comp_max_correction = 1.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.5f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Normal_SetsNormalPitchComp) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Normal;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.01f);
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.25f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Sport_SetsSportPitchComp) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Sport;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.02f);
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.30f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Drift_SetsDriftPitchComp) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Drift;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.pitch_comp_gain, 0.005f);
  EXPECT_FLOAT_EQ(cfg.pitch_comp_max_correction, 0.15f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_DoesNotChangePitchCompEnabled) {
  StabilizationConfig cfg{};
  cfg.pitch_comp_enabled = true;
  cfg.mode = DriveMode::Normal;
  cfg.ApplyModeDefaults();
  EXPECT_TRUE(cfg.pitch_comp_enabled);
}

TEST(StabilizationConfigTest, SportPitchGainStrongerThanNormal) {
  StabilizationConfig normal{};
  normal.mode = DriveMode::Normal;
  normal.ApplyModeDefaults();

  StabilizationConfig sport{};
  sport.mode = DriveMode::Sport;
  sport.ApplyModeDefaults();

  EXPECT_GT(sport.pitch_comp_gain, normal.pitch_comp_gain);
  EXPECT_GT(sport.pitch_comp_max_correction, normal.pitch_comp_max_correction);
}

TEST(StabilizationConfigTest, DriftPitchGainSofterThanNormal) {
  StabilizationConfig normal{};
  normal.mode = DriveMode::Normal;
  normal.ApplyModeDefaults();

  StabilizationConfig drift{};
  drift.mode = DriveMode::Drift;
  drift.ApplyModeDefaults();

  EXPECT_LT(drift.pitch_comp_gain, normal.pitch_comp_gain);
  EXPECT_LT(drift.pitch_comp_max_correction, normal.pitch_comp_max_correction);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slip PID Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(StabilizationConfigTest, DefaultSlipTargetIsZero) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.slip_target_deg, 0.0f);
}

TEST(StabilizationConfigTest, DefaultSlipKpIsZero) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.slip_kp, 0.0f);
  EXPECT_FLOAT_EQ(cfg.slip_ki, 0.0f);
  EXPECT_FLOAT_EQ(cfg.slip_kd, 0.0f);
}

TEST(StabilizationConfigTest, DefaultSlipMaxCorrectionIsZero) {
  StabilizationConfig cfg{};
  EXPECT_FLOAT_EQ(cfg.slip_max_correction, 0.0f);
}

TEST(StabilizationConfigTest, ResetRestoresSlipDefaults) {
  StabilizationConfig cfg{};
  cfg.slip_target_deg = 20.0f;
  cfg.slip_kp = 0.1f;
  cfg.slip_max_correction = 0.5f;
  cfg.Reset();
  EXPECT_FLOAT_EQ(cfg.slip_target_deg, 0.0f);
  EXPECT_FLOAT_EQ(cfg.slip_kp, 0.0f);
  EXPECT_FLOAT_EQ(cfg.slip_max_correction, 0.0f);
}

TEST(StabilizationConfigTest, Clamp_SlipKpNegative_ClampedToZero) {
  StabilizationConfig cfg{};
  cfg.slip_kp = -0.5f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.slip_kp, 0.0f);
}

TEST(StabilizationConfigTest, Clamp_SlipKpTooHigh_ClampedTo1) {
  StabilizationConfig cfg{};
  cfg.slip_kp = 5.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.slip_kp, 1.0f);
}

TEST(StabilizationConfigTest, Clamp_SlipTargetTooHigh_ClampedTo45) {
  StabilizationConfig cfg{};
  cfg.slip_target_deg = 90.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.slip_target_deg, 45.0f);
}

TEST(StabilizationConfigTest, Clamp_SlipTargetTooLow_ClampedToNeg45) {
  StabilizationConfig cfg{};
  cfg.slip_target_deg = -90.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.slip_target_deg, -45.0f);
}

TEST(StabilizationConfigTest, Clamp_SlipMaxCorrTooHigh_ClampedTo1) {
  StabilizationConfig cfg{};
  cfg.slip_max_correction = 2.0f;
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.slip_max_correction, 1.0f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Normal_SlipPidDisabled) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Normal;
  cfg.ApplyModeDefaults();
  EXPECT_FLOAT_EQ(cfg.slip_kp, 0.0f);
  EXPECT_FLOAT_EQ(cfg.slip_max_correction, 0.0f);
  EXPECT_FLOAT_EQ(cfg.slip_target_deg, 0.0f);
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Drift_SlipPidEnabled) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Drift;
  cfg.ApplyModeDefaults();
  EXPECT_GT(cfg.slip_kp, 0.0f) << "Drift mode must have non-zero slip_kp";
  EXPECT_GT(cfg.slip_max_correction, 0.0f) << "Drift mode must allow slip correction";
  EXPECT_GT(cfg.slip_target_deg, 0.0f) << "Drift mode must have non-zero target slip";
}

TEST(StabilizationConfigTest, ApplyModeDefaults_DriftSlipStrongerThanSport) {
  StabilizationConfig sport{};
  sport.mode = DriveMode::Sport;
  sport.ApplyModeDefaults();

  StabilizationConfig drift{};
  drift.mode = DriveMode::Drift;
  drift.ApplyModeDefaults();

  EXPECT_GT(drift.slip_kp, sport.slip_kp)
      << "Drift mode should have stronger slip PID than sport";
  EXPECT_GT(drift.slip_target_deg, sport.slip_target_deg)
      << "Drift mode should have larger target slip angle than sport";
}

TEST(StabilizationConfigTest, ApplyModeDefaults_Sport_SlipPidLight) {
  StabilizationConfig cfg{};
  cfg.mode = DriveMode::Sport;
  cfg.ApplyModeDefaults();
  // Sport имеет небольшой slip kp (лёгкий ассист)
  EXPECT_GT(cfg.slip_kp, 0.0f);
  EXPECT_LT(cfg.slip_kp, 0.01f) << "Sport slip kp should be small";
}

// ─────────────────────────────────────────────────────────────────────────────
// Adaptive PID Tests (Phase 4.1)
// ─────────────────────────────────────────────────────────────────────────────

TEST(StabilizationConfigTest, AdaptivePid_DefaultsCorrect) {
  StabilizationConfig cfg{};
  EXPECT_FALSE(cfg.adaptive_pid_enabled);
  EXPECT_FLOAT_EQ(cfg.adaptive_speed_ref_ms, 1.5f);
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_min, 0.5f);
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_max, 2.0f);
}

TEST(StabilizationConfigTest, AdaptivePid_ClampValues) {
  StabilizationConfig cfg{};
  cfg.adaptive_speed_ref_ms = 0.01f;  // ниже минимума 0.1
  cfg.adaptive_scale_min = 0.05f;    // ниже минимума 0.1
  cfg.adaptive_scale_max = 10.0f;    // выше максимума 5.0
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.adaptive_speed_ref_ms, 0.1f);
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_min, 0.1f);
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_max, 5.0f);
}

TEST(StabilizationConfigTest, AdaptivePid_ClampScaleMinNotAboveOne) {
  StabilizationConfig cfg{};
  cfg.adaptive_scale_min = 1.5f;  // выше максимума 1.0
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_min, 1.0f);
}

TEST(StabilizationConfigTest, AdaptivePid_ClampScaleMaxNotBelowOne) {
  StabilizationConfig cfg{};
  cfg.adaptive_scale_max = 0.8f;  // ниже минимума 1.0
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_max, 1.0f);
}

TEST(StabilizationConfigTest, AdaptivePid_ResetRestoresDefaults) {
  StabilizationConfig cfg{};
  cfg.adaptive_pid_enabled = true;
  cfg.adaptive_speed_ref_ms = 5.0f;
  cfg.adaptive_scale_min = 0.2f;
  cfg.adaptive_scale_max = 4.0f;
  cfg.Reset();
  EXPECT_FALSE(cfg.adaptive_pid_enabled);
  EXPECT_FLOAT_EQ(cfg.adaptive_speed_ref_ms, 1.5f);
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_min, 0.5f);
  EXPECT_FLOAT_EQ(cfg.adaptive_scale_max, 2.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Oversteer Warning Tests (Phase 4.2)
// ─────────────────────────────────────────────────────────────────────────────

TEST(StabilizationConfigTest, OversteerWarn_DefaultsCorrect) {
  StabilizationConfig cfg{};
  EXPECT_FALSE(cfg.oversteer_warn_enabled);
  EXPECT_FLOAT_EQ(cfg.oversteer_slip_thresh_deg, 20.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_rate_thresh_deg_s, 50.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_throttle_reduction, 0.0f);
}

TEST(StabilizationConfigTest, OversteerWarn_ClampValues) {
  StabilizationConfig cfg{};
  cfg.oversteer_slip_thresh_deg = 1.0f;    // ниже минимума 5
  cfg.oversteer_rate_thresh_deg_s = 5.0f;  // ниже минимума 10
  cfg.oversteer_throttle_reduction = 2.0f; // выше максимума 1
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.oversteer_slip_thresh_deg, 5.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_rate_thresh_deg_s, 10.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_throttle_reduction, 1.0f);
}

TEST(StabilizationConfigTest, OversteerWarn_ClampUpperBounds) {
  StabilizationConfig cfg{};
  cfg.oversteer_slip_thresh_deg = 100.0f;   // выше максимума 45
  cfg.oversteer_rate_thresh_deg_s = 1000.0f; // выше максимума 500
  cfg.oversteer_throttle_reduction = -0.5f;  // ниже минимума 0
  cfg.Clamp();
  EXPECT_FLOAT_EQ(cfg.oversteer_slip_thresh_deg, 45.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_rate_thresh_deg_s, 500.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_throttle_reduction, 0.0f);
}

TEST(StabilizationConfigTest, OversteerWarn_ResetRestoresDefaults) {
  StabilizationConfig cfg{};
  cfg.oversteer_warn_enabled = true;
  cfg.oversteer_slip_thresh_deg = 30.0f;
  cfg.oversteer_rate_thresh_deg_s = 100.0f;
  cfg.oversteer_throttle_reduction = 0.5f;
  cfg.Reset();
  EXPECT_FALSE(cfg.oversteer_warn_enabled);
  EXPECT_FLOAT_EQ(cfg.oversteer_slip_thresh_deg, 20.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_rate_thresh_deg_s, 50.0f);
  EXPECT_FLOAT_EQ(cfg.oversteer_throttle_reduction, 0.0f);
}
