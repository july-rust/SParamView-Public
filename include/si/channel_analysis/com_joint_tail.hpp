#pragma once
// 012_20: Bounded, NON-NORMATIVE receiver-referenced statistical mixture.
// This is NOT IEEE 802.3ck Annex 93A COM, FOM, DER qualification or PHY verdict.
#include "si/channel_analysis/common.hpp"
#include <atomic>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace si::channel {

enum class ComJointComponentKind {
  ReceiverGaussian, RandomJitterGaussian, DualDiracPeak, IndependentPam4Tap
};

struct ComJointVoltageEvidence {
  double amplitudeV = std::numeric_limits<double>::quiet_NaN();
  ComJointComponentKind kind = ComJointComponentKind::IndependentPam4Tap;
  std::string componentId, testCase, referencePlane, calibrationIdentity;
  // Explicit model assumption, not a proof of actual physical independence.
  bool assumedIndependent = false;
  // True only for the receiver-Gaussian component. Reject upstream combined
  // jitter/RX-noise values used again as independent RJ components.
  bool includesReceiverGaussianNoise = false;
};

struct ComJointTailInput {
  Result<ComJointVoltageEvidence> receiverGaussian;
  Result<ComJointVoltageEvidence> randomJitterGaussian;
  Result<ComJointVoltageEvidence> dualDiracPeak;
  std::vector<Result<ComJointVoltageEvidence>> pam4Taps;
  double oneSidedTailProbability = std::numeric_limits<double>::quiet_NaN();
};

struct ComJointTailData {
  double oneSidedNoiseQuantileV = 0;
  double verifiedTailProbability = 0;
  double gaussianVarianceV2 = 0, totalVarianceV2 = 0;
  size_t statesEvaluated = 0;
  std::string testCase, referencePlane, calibrationIdentity;
  std::string model = "EXPLORATORY_INDEPENDENT_PAM4_DUAL_DIRAC_GAUSSIAN";
};

struct ComJointTailCasesData {
  ComJointTailData test1, test2;
  double worstCaseNoiseQuantileV = 0; // maximum noise, NOT IEEE COM worst case
  std::string model = "EXPLORATORY_TWO_CASE_JOINT_TAIL_NOT_IEEE_COM";
};

// Upstream must independently calibrate all components to the SAME receiver
// decision plane and time. This utility cannot infer physical calibration
// from normalized symbol pulses or certify the claimed independence.
// Throws si::Error on bad inputs, si::Cancelled on cancellation.
// Every usable output is Warning + NOT_IEEE_COM; never an official COM result.
Result<ComJointTailData> calculateComJointTail(
    const ComJointTailInput& input, const std::string& testCase,
    const Provenance& expected, const std::atomic_bool* cancelled = nullptr);

Result<ComJointTailCasesData> calculateComJointTailCases(
    const ComJointTailInput& test1, const ComJointTailInput& test2,
    const Provenance& expectedTest1, const Provenance& expectedTest2,
    const std::atomic_bool* cancelled = nullptr);

} // namespace si::channel
