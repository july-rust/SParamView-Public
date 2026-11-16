#pragma once
// 012_07: phase-preserving, band-limited, deterministic NRZ channel-eye model.
// Numerical exploration ONLY: never a measured eye, BER, or PHY conformance.
#include "si/channel_analysis/common.hpp"
#include <atomic>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

enum class EyeProfile {
  MipiDphyHs, Ddr2, Ddr3, Ddr4, Ddr5, Lvds, Slvs, SlvsEc, Custom
};
enum class EyeMode { Unspecified, SingleEnded, Differential };
enum class EyePattern { Alternating, Prbs7, CustomBits };

// All electrical fields default to NaN: no invented PHY voltage, termination,
// source impedance, threshold or RX input range. Voltage denotes the incident
// single-ended or differential voltage at the S-parameter reference plane.
// S21/Sdd21 is a voltage wave ratio ONLY for a matched two-port with equal Zref.
struct EyeElectrical {
  double txLowV = std::numeric_limits<double>::quiet_NaN();
  double txHighV = std::numeric_limits<double>::quiet_NaN();
  double txSourceOhm = std::numeric_limits<double>::quiet_NaN();
  double rxTerminationOhm = std::numeric_limits<double>::quiet_NaN();
  double rxThresholdV = std::numeric_limits<double>::quiet_NaN();
  double rxInputMinimumV = std::numeric_limits<double>::quiet_NaN();
  double rxInputMaximumV = std::numeric_limits<double>::quiet_NaN();
};
struct EyeSettings {
  EyeProfile profile = EyeProfile::Custom;
  EyeMode mode = EyeMode::Unspecified;
  EyePattern pattern = EyePattern::Prbs7;
  // Independent quantities: baud (UI), reference marker, measured analysis cap.
  double bitRateHz = std::numeric_limits<double>::quiet_NaN();
  double referenceFrequencyHz = std::numeric_limits<double>::quiet_NaN();
  double analysisStopHz = std::numeric_limits<double>::quiet_NaN();
  EyeElectrical electrical;
  size_t fftLength = 8192;     // radix-2, sample-rate = baud * samplesPerUI
  size_t samplesPerUI = 32;    // radix-2, minimum 8, integer symbol edges
  std::vector<uint8_t> customBits;
};
// Only MIPI example supplies bit rate (4.5 Gb/s), reference Nyquist marker
// (2.25 GHz), and analysis cap (10 GHz). All profile electrical fields remain
// unspecified. Other profiles intentionally require explicit rates/bands.
EyeSettings eyeProfile(EyeProfile profile);
const char *eyeProfileName(EyeProfile profile);

struct EyeData {
  std::vector<double> timeSeconds, transmittedV, receivedV;
  TimeSeries impulse, unitPulse;
  // A bounded subset of two-UI received traces, for the 012_08 GUI renderer.
  std::vector<std::vector<double>> overlayV;
  std::vector<double> overlayPhaseUI;
  double bitRateHz = 0, referenceFrequencyHz = 0, analysisStopHz = 0;
  double unitIntervalSeconds = 0, timeStepSeconds = 0;
  double samplingPhaseUI = 0, estimatedDelaySeconds = 0;
  double eyeHeightV = 0, eyeWidthSeconds = 0, eyeWidthUI = 0;
  double minimumHighV = 0, maximumLowV = 0;
  double minimumReceivedV = 0, maximumReceivedV = 0;
  bool bandLimited = false, simulated = true;
  std::string profileName, transferParameter;
  // Deterministic worst-case sampled high/low separation for the GIVEN
  // periodic bit sequence; never an eye-mask, statistical BER, or compliance.
};
Result<EyeData> calculateEye(const ComplexTrace &through,
                             const EyeSettings &settings,
                             Provenance provenance = {},
                             const std::atomic_bool *cancelled = nullptr);
} // namespace si::channel
