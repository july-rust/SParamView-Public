#pragma once
// 012_06: exploratory, band-limited uncorrelated NRZ crosstalk integration.
// This is NOT a standard-specific IEEE ICN / SNIA ccICN / COM implementation.
#include "si/channel_analysis/common.hpp"
#include <atomic>

namespace si::channel {
enum class IcnEnd { Next, Fext };
struct IcnAggressorInput {
  std::string id;
  IcnEnd end = IcnEnd::Next;
  ComplexTrace transfer;
};
struct IcnSettings {
  double startHz = 0;
  double stopHz = 30e9;
  double symbolRateHz = 4e9;      // iid equiprobable two-level symbols
  double txPeakToPeakV = 0.8;     // Vpp, voltage across the selected mode
  double riseTimeSeconds = 30e-12; // Gaussian 10%-90% edge time
  double rxBandwidthHz = 10e9;   // single-pole voltage 3-dB bandwidth
};
// Measured band and explicit generic assumed NRZ electrical conditions.
IcnSettings automaticIcnSettings(const ComplexTrace &trace);
struct IcnAggressorData {
  std::string id, parameter;
  IcnEnd end = IcnEnd::Next;
  double rmsV = 0, powerPercent = 0;
  std::vector<double> spectralDensityV2Hz, cumulativeRmsV;
};
struct IcnData {
  std::vector<double> frequencyHz;
  std::vector<double> nextPowerDb, fextPowerDb;
  std::vector<double> nextDensityV2Hz, fextDensityV2Hz;
  std::vector<double> nextCumulativeRmsV, fextCumulativeRmsV,
                      totalCumulativeRmsV;
  std::vector<IcnAggressorData> aggressors;
  double nextRmsV = 0, fextRmsV = 0, totalRmsV = 0;
  double actualStartHz = 0, actualStopHz = 0;
  IcnSettings settings; // immutable calculation settings for reproducible export
  std::string victimId, direction; // qualified GUI snapshot context, if available
  bool bandLimited = false;
  std::string analysisMode = "MANUAL", configurationReason = "USER_SPECIFIED";
  std::string model = "Generic IID NRZ, one-sided PSD, Gaussian 10-90 edge, one-pole RX";
};
Result<IcnData> calculateIcn(const std::vector<IcnAggressorInput> &aggressors,
                             const IcnSettings &settings,
                             Provenance provenance = {},
                             const std::atomic_bool *cancelled = nullptr);
} // namespace si::channel
