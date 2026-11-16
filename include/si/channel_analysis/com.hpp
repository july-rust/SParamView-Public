#pragma once
// 012_09: COM phase-preserving input preparation; not an IEEE COM solver.
// A normative IEEE 802.3ck COM margin must not be reported until the complete
// approved Table 120F-8, both package tests and independent golden qualify.
#include "si/channel_analysis/common.hpp"
#include <atomic>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

enum class ComCoupling { Next, Fext };
struct ComAggressor {
  std::string id;
  ComCoupling coupling = ComCoupling::Next;
  ComplexTrace transfer;
};
struct ComSettings {
  std::string profileId =
    "IEEE-802.3ck-2022-100GAUI-1-C2C-120F-REFERENCE-CANDIDATE";
  double symbolRateHz = std::numeric_limits<double>::quiet_NaN();
  double referenceOhm = std::numeric_limits<double>::quiet_NaN();
  // Explicit dimensionless FIR coefficients; k-th tap delayed k symbol UIs.
  // An unqualified coefficient set cannot be silently called IEEE TX FFE.
  std::vector<double> txFfeTaps;
};
struct ComInput {
  ComplexTrace thru;
  ComplexTrace rxFilter;
  std::vector<ComAggressor> aggressors;
};
struct ComPreparedAggressor {
  std::string id;
  ComCoupling coupling = ComCoupling::Next;
  std::vector<Complex> conditionedTransfer;
};
struct ComPreparedData {
  std::vector<double> frequencyHz;
  std::vector<Complex> txFfeResponse, conditionedThru;
  std::vector<ComPreparedAggressor> aggressors;
  // This is a matched-reference, scalar frequency diagnostic only.
  // It has no package-network cascade, IEEE noise, COM, or PHY verdict.
  std::string model = "EXPLORATORY_MATCHED_WAVE_SPECTRAL_PREPARATION";
};
struct ComMarginData {
  double comDb = std::numeric_limits<double>::quiet_NaN();
  double test1Db = std::numeric_limits<double>::quiet_NaN();
  double test2Db = std::numeric_limits<double>::quiet_NaN();
};
// Preserves complex phase; rejects missing DC, grids, impedances, parameters,
// nonfinite EQ/FFE, and unsupported profiles. Not a standards calculation.
Result<ComPreparedData> prepareComSpectra(const ComInput &input,
    const ComSettings &settings, Provenance provenance = {},
    const std::atomic_bool *cancelled = nullptr);
// Normative solver deliberately closed while approved 802.3ck package/noise
// parameters and 3 independent same-profile Test1/Test2 golden sets are absent.
// Never return a fabricated finite COM result.
Result<ComMarginData> calculateCom(const ComInput &input,
    const ComSettings &settings, Provenance provenance = {},
    const std::atomic_bool *cancelled = nullptr);

} // namespace si::channel
