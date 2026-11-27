#pragma once
// 012_19: Mathematical voltage-ratio diagnostics; not normative IEEE COM.
#include "si/channel_analysis/common.hpp"
#include <atomic>
#include <limits>
#include <string>
#include <string_view>

namespace si::channel {
struct ComVoltageEvidence {
  double amplitudeV = std::numeric_limits<double>::quiet_NaN();
  std::string testCase, referencePlane, calibrationIdentity;
};
struct ComVoltageRatioInput {
  Result<ComVoltageEvidence> signal, noiseAtTargetDer;
};
struct ComVoltageRatioData {
  double signalAmplitudeV = 0, noiseAmplitudeV = 0, ratioDb = 0;
  std::string testCase, referencePlane, calibrationIdentity;
  std::string model = "EXPLORATORY_VOLTAGE_RATIO_NOT_IEEE_COM";
};
struct ComVoltageRatioCasesData {
  ComVoltageRatioData test1, test2;
  double minimumRatioDb = 0;
  std::string model = "EXPLORATORY_TWO_CASE_VOLTAGE_RATIO_NOT_IEEE_COM";
};
// Only evaluates 20*log10(signal/noise). It does not generate ComMarginData,
// IEEE-qualified amplitudes, a COM value, or a PHY verdict.
// All successful outputs are Warning + NOT_IEEE_COM.
// Pure numeric dB kernel shared with 012_28. Validates positive finite V.
double calculateDiagnosticVoltageRatioDb(double signalV, double noiseV);

Result<ComVoltageRatioData> calculateComDiagnosticVoltageRatio(
    const ComVoltageRatioInput& input, std::string_view testCase,
    const Provenance& expected,
    const std::atomic_bool* cancelled = nullptr);
Result<ComVoltageRatioCasesData> calculateComDiagnosticVoltageRatioCases(
    const ComVoltageRatioInput& test1, const ComVoltageRatioInput& test2,
    const Provenance& expectedTest1, const Provenance& expectedTest2,
    const std::atomic_bool* cancelled = nullptr);
} // namespace si::channel
