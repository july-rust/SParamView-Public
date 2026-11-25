#pragma once
// 012_05: exploratory insertion-loss deviation (not IEEE FOM_ILD).
#include "si/channel_analysis/common.hpp"
#include <atomic>
#include <array>

namespace si::channel {
enum class IldFitModel { Linear, RootLinear, RootLinearQuadratic };
struct IldSettings {
  double startHz = 0;
  double stopHz = 30e9;
  IldFitModel model = IldFitModel::RootLinear;
};
struct IldData {
  std::vector<double> frequencyHz, insertionLossDb, fittedLossDb, deviationDb;
  std::array<double,4> coefficients{}; // normalized f/fScale; unused = 0
  double frequencyScaleHz = 0;
  double rangeStartHz = 0, rangeStopHz = 0;
  double requestedStartHz = 0, requestedStopHz = 0;
  double minimumDb = 0, maximumDb = 0, peakAbsoluteDb = 0, rmsDb = 0;
  double peakFrequencyHz = 0;
  std::string parameter, modelName;
};
Result<IldData> calculateIld(const ComplexTrace &trace, const IldSettings &settings,
                             Provenance provenance = {},
                             const std::atomic_bool *cancelled = nullptr);
const char *ildModelName(IldFitModel model);
} // namespace si::channel
