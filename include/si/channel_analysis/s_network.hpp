#pragma once
// 012_09: passive matched-reference two-port cascade and 4-port mixed-mode
// extraction. Exact network algebra, not a COM margin/802.3ck qualification.
#include "si/channel_analysis/two_port.hpp"
#include <array>
#include <limits>
namespace si::channel {
struct ComWaveTwoPort {
  ComTwoPortS s;
  // Real positive reference impedance of both external wave ports.
  double referenceOhm = std::numeric_limits<double>::quiet_NaN();
};
struct ComPhysicalFourPort {
  // Row major: S[response*4 + stimulus], zero-based physical ports
  // nearP, nearN, farP, farN respectively.
  std::array<Complex,16> scattering{};
  std::array<double,4> referenceOhm{};
};
struct ComDifferentialExtraction {
  ComWaveTwoPort differential;
  double maximumModeConversion = 0;
};
// Passive, complex, bidirectional Redheffer two-port star product. Includes
// multiple internal reflections; rejects mismatched Zref/active/singular data.
ComWaveTwoPort cascadeComScattering(const ComWaveTwoPort &left,
                                     const ComWaveTwoPort &right);
// Construct a two-port from the closed reciprocal ABCD primitives.
ComWaveTwoPort toComWave(const ComTwoPort &network,
                         double referenceOhm);
// Full 4-port physical wave -> differential 2-port using unitary mixed-mode
// transform; does not infer physical mode from a single scalar THRU trace.
// Rejects nonpassive inputs and mode-conversion above explicit caller bound.
// Physical references must be equal; differential wave reference is 2*Zref.
ComDifferentialExtraction extractDifferentialComNetwork(
    const ComPhysicalFourPort &physical,
    double maximumAllowedModeConversion = 0);
} // namespace si::channel
