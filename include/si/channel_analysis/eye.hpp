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
  // NRZ referenceFrequencyHz == bitRateHz/2; analysisStopHz is independent.
  double bitRateHz = std::numeric_limits<double>::quiet_NaN();
  double referenceFrequencyHz = std::numeric_limits<double>::quiet_NaN();
  double analysisStopHz = std::numeric_limits<double>::quiet_NaN();
  EyeElectrical electrical;
  size_t fftLength = 8192;     // radix-2, sample-rate = baud * samplesPerUI
  size_t samplesPerUI = 32;    // radix-2, minimum 8, integer symbol edges
  std::vector<uint8_t> customBits;
  bool acceptLimitedDc = false; // explicit acknowledgement for >10 kHz and <100 kHz
};
// Only MIPI example supplies bit rate (4.5 Gb/s), NRZ Nyquist reference
// (2.25 GHz), and analysis cap (10 GHz). All profile electrical fields remain
// unspecified. Other profiles intentionally require explicit rates/bands.
EyeSettings eyeProfile(EyeProfile profile);
// Adjusting the NRZ reference MUST also change symbol rate and UI.
EyeSettings eyeWithReferenceFrequency(EyeSettings settings, double referenceHz);
const char *eyeProfileName(EyeProfile profile);

// Eye-only DC conditioning; does not alter source, TDR or other analyzers.
struct EyeDcInfo {
  double originalStartHz = 0, effectiveStartHz = 0, firstPositiveHz = 0;
  double value = 0, modelSpread = 0;
  std::string status = "ORIGINAL DC", method = "Original";
  bool requiresConfirmation = false, limited = false, modelSpreadAvailable = false;
  // Observed one-pole AC-like transfer, not proof of physical capacitor topology.
  bool acModel = false;
  double acPoleHz = 0, acGain = 0, acFitMaxResidual = 0;
  // Analytic one-pole droop is not full measured-channel long-pattern qualification.
};
// Throws on unsupported/invalid data; a sparse original real DC may be retained
// as unqualified, never advertised as verified physical DC.
EyeDcInfo inspectEyeDc(const ComplexTrace &trace);
struct EyeData {
  EyeDcInfo dc;
  std::vector<double> timeSeconds, transmittedV, receivedV;
  TimeSeries impulse, unitPulse;
  // A bounded subset of two-UI received traces, for the 012_08 GUI renderer.
  std::vector<std::vector<double>> overlayV;
  std::vector<double> overlayPhaseUI;
  double bitRateHz = 0, referenceFrequencyHz = 0, analysisStopHz = 0;
  double unitIntervalSeconds = 0, timeStepSeconds = 0;
  double acTimeConstantSeconds = 0, acFrameDroopFraction = 0;
  double samplingPhaseUI = 0, estimatedDelaySeconds = 0;
  double eyeHeightV = 0, eyeWidthSeconds = 0, eyeWidthUI = 0;
  double minimumHighV = 0, maximumLowV = 0;
  double minimumReceivedV = 0, maximumReceivedV = 0;
  EyeSettings settings;
  size_t frameBitCount = 0;
  bool samplingPhaseThresholdOpen = false;
  bool bandLimited = false, simulated = true;
  std::string victimId; // populated by the verified source adapter
  bool reverse = false;
  std::string profileName, transferParameter;
  // Deterministic worst-case sampled high/low separation for the GIVEN
  // periodic bit sequence; never an eye-mask, statistical BER, or compliance.
};
Result<EyeData> calculateEye(const ComplexTrace &through,
                             const EyeSettings &settings,
                             Provenance provenance = {},
                             const std::atomic_bool *cancelled = nullptr);
} // namespace si::channel
