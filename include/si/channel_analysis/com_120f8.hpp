#pragma once
// Numeric contract for the PUBLISHED IEEE Std 802.3ck-2022 Annex 120F,
// Table 120F-8 (100GAUI-1/200GAUI-2/400GAUI-4 C2C).
// This file records parameter values only. It is NOT an Annex 93A COM solver
// or evidence that a measured channel qualifies for PHY PASS/FAIL.
// Primary standard: https://ieeexplore.ieee.org/document/9999414
#include "si/channel_analysis/com.hpp"
#include "si/channel_analysis/com_physical_channel.hpp"
#include <array>
#include <vector>

namespace si::channel {
struct Com120F8DiscreteRange {
  double minimum, maximum, step;
};

struct Com120F8Parameters {
  double symbolRateHz = 53.125e9;
  double maximumStartFrequencyHz = 0.05e9;
  double maximumFrequencyStepHz = 0.01e9;
  double minimumComDb = 3.0;
  double txTransitionTimeS = 7.5e-12; // Annex 120F.4.1, Ht(f)
  double referenceOhmSingleEnded = 50.0;
  double receiverTerminationOhmSingleEnded = 50.0;
  double cdFaradSingleEnded = 1.2e-13;
  double lsHenrySingleEnded = 1.2e-10;
  double cbFaradSingleEnded = 3.0e-14;
  double cpFaradSingleEnded = 8.7e-14;
  std::array<double, 2> txPackageLengthMm{13.,31.};
  std::array<double, 2> rxPackageLengthMm{11.,29.};
  double secondaryPackageLengthMm = 1.8;
  double primaryPackageZcOhm = 87.5;
  double secondaryPackageZcOhm = 92.5;
  double packageA1SqrtNsPerMm = 9.909e-4;
  double packageA2NsPerMm = 2.772e-4;
  double receiverBandwidthHz = 0.75*53.125e9;
  // Ordering: pre(-3), pre(-2), pre(-1), post(+1). c(0) separately.
  std::array<Com120F8DiscreteRange,4> txNoncursor{
      Com120F8DiscreteRange{-0.04,0.0,0.02},
      Com120F8DiscreteRange{0.0,0.10,0.02},
      Com120F8DiscreteRange{-0.28,0.0,0.02},
      Com120F8DiscreteRange{-0.10,0.0,0.02}};
  double minimumCursor = 0.54;
  Com120F8DiscreteRange ctleDcGainDb{-20.,0.,1.};
  Com120F8DiscreteRange ctleDcGain2Db{-4.,0.,1.};
  double ctleZeroFrequencyHz = 53.125e9/2.5;
  double ctlePole1FrequencyHz = 53.125e9/2.5;
  double ctlePole2FrequencyHz = 53.125e9;
  double ctleLowFrequencyPoleZeroHz = 53.125e9/80.;
  double victimPeakV = 0.413;
  double fextPeakV = 0.413;
  double nextPeakV = 0.608;
  int pamLevels = 4;
  double minimumRlm = 0.95;
  double txSnrDb = 33.;
  int samplesPerUi = 32;
  int dfeTaps = 6;
  std::array<double,6> dfeMin{0.3,0.05,-0.04,-0.04,-0.04,-0.04};
  std::array<double,6> dfeMax{0.65,0.15,0.1,0.1,0.1,0.1};
  double randomJitterRmsUi = 0.01;
  double dualDiracJitterPeakUi = 0.02;
  double oneSidedNoiseSpectralDensityV2PerGHz = 2e-8;
  double targetDetectorErrorRatio = 1e-5;
};

// Immutable by value. A different PHY or revision MUST use a distinct type
// and audit; do not silently accept draft-era configuration overrides.
constexpr Com120F8Parameters final8023ck120F8() { return {}; }

struct Com120F8EqualizerChoice {
  // Index 0..4 = c(-3), c(-2), c(-1), c(0), c(+1).
  std::array<double,5> txFfe{0.,0.,0.,1.,0.};
  double ctleDcGainDb = 0.;
  double ctleDcGain2Db = 0.;
};

// Validates ONLY documented discrete TX/CTLE ranges, NOT the PHY COM result.
// Bounds and quantization are strict; no rounding of off-grid choices.
void validateCom120F8Equalizer(const Com120F8EqualizerChoice& choice);

// Annex 93A.1 CTLE transfer as parameterized for final 802.3ck 120F-8.
// Complex transfer includes phase. The fourth-order receive noise filter
// is evaluated separately; both remain components, NOT complete COM margin.
// IEEE 93A-46 Gaussian TX transition response, with Tr=7.5 ps
// (120F C2C) and beta=1. Independent real-valued component, not COM.
double com120F8TxTransitionFilter(double frequencyHz);
Complex com120F8Ctle(double frequencyHz, double gDcDb, double gDc2Db);
Complex com120F8ReceiverNoiseFilter(double frequencyHz);
Complex com120F8ReceiverTransfer(double frequencyHz,
                                 double gDcDb, double gDc2Db);

// Builds the actual C++ 120F-8 fixed TX FFE + 93A CTLE +
 // fourth-order receive-filter complex diagnostic on the supplied THRU,
 // FEXT and NEXT complex traces. The input RX filter must be omitted and is
 // generated from the final profile, never silently overwritten.
 // This calls the non-normative preparation engine; NO COM ratio/verdict.
Result<ComPreparedData> prepareCom120F8ReceiverSpectra(
    ComInput input, const Com120F8EqualizerChoice& choice,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);

// Checks sample identity for a direct grid (starting at/below fmin, maximum
// step and Nyquist coverage). Interpolation/extrapolation is NOT performed.
// This is a deliberately strict subset of usable COM inputs, not the complete
// reference implementation's frequency handling.
void validateCom120F8DirectGrid(const std::vector<double>& frequencyHz);


// A bounded, explicitly NON-NORMATIVE time-domain primitive for 012_09_03.
// The caller supplies the already phase-conditioned 120F-8 THRU spectrum.
// No frequency padding, extrapolation, package substitution, statistical
// noise, equalizer optimization, COM dB, or standard PHY verdict is performed.
struct Com120F8PulseDiagnostics {
  double samplePeriodS = 0;
  double mainCursorTimeS = 0;
  double mainCursorAmplitude = 0; // dimensionless, not an assumed voltage
  size_t mainCursorSample = 0;
  std::vector<double> uiTapOffsets;  // -3,-2,-1,0,+1,...,+9 UI
  std::vector<double> uiTapAmplitudes; // signed, phase-aware time response
  const char* model = "EXPLORATORY_BANDLIMITED_CIRCULAR_PULSE";
};

// Strict direct radix-2 positive spectrum with real DC/Nyquist endpoints,
// zero additional interpolation, no hidden periodic wrap into the reported
// tap window, and an independent continuous-time DFT sampling of cursor taps.
// Data is inherently periodic/band-limited: NEVER equate it to Annex 93A COM.
Result<Com120F8PulseDiagnostics> calculateCom120F8BandlimitedPulse(
    const ComPreparedData& prepared, Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);


// 012_09_03: bridge BOTH already-loaded, physically cascaded Test 1/2
// differential networks to the native 120F-8 FFE/CTLE/receiver diagnostics.
// Preserves each complex Sdd21 and explicit 13/11 vs 31/29 mm package
// identity. No mixed-mode data are reconstructed from Sdd21 and no channel
// or noise reference is guessed. This is NOT IEEE COM dB or PASS/FAIL.
struct Com120F8LoadedTestCaseSpectra {
  ComPreparedData test1, test2;
  const char* model = "EXPLORATORY_C2C_PACKAGE_HT_FFE_CTLE_RX";
};
Result<Com120F8LoadedTestCaseSpectra> prepareCom120F8LoadedTestCases(
    const ComPhysicalChannelData& physical,
    const Com120F8EqualizerChoice& choice,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);


// 012_09_03 — actual ideal PAM4/NRZ *rectangular symbol* convolution with
// already-conditioned complex H(f). This integrates 93A's ideal symbol
// excitation X(f) = UI*sinc(pi*f*UI)*exp(-j*pi*f*UI), over the measured band.
// H(f) is the dimensionless, matched-response transfer. Output is a
// dimensionless pulse per unit symbol level, NOT a calibrated voltage, BER,
// statistical COM, PHY PASS/FAIL, or a normative Annex 93A qualification.
// Query sample times are explicit — no inferred channel delay or sample phase.
struct Com120F8RectangularPulseData {
  std::vector<double> timeSeconds;
  std::vector<double> unitSymbolPulse;
  double symbolPeriodSeconds = 0;
  double maximumAbsolutePulse = 0;
  const char* model = "EXPLORATORY_MEASURED_BAND_RECTANGULAR_SYMBOL_CONVOLUTION";
};
Result<Com120F8RectangularPulseData> calculateCom120F8RectangularPulse(
    const ComPreparedData& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);

// Keeps Test1 and Test2 physically distinct through identical sampled-time
// symbol excitation, without claiming an IEEE 802.3ck COM dB result.
struct Com120F8RectangularPulseCases {
  Com120F8RectangularPulseData test1, test2;
};
Result<Com120F8RectangularPulseCases> calculateCom120F8RectangularPulseCases(
    const Com120F8LoadedTestCaseSpectra& conditioned,
    const std::vector<double>& queryTimeSeconds,
    Provenance provenance = {},
    const std::atomic_bool* cancelled = nullptr);

// 012_33: source-anchored *historical causal* Test1/Test2 physical-to-RX
// complex bridge. Unlike the lower-level input adapter, this entrypoint
// constructs the historical package from the confirmed S4P SourceSnapshot.
// observedLiveIdentity is supplied by the caller and checked before/after;
// callers MUST recheck their actual live workspace identity at presentation.
// Always Warning/NOT_IEEE_COM; never a production PHY/COM margin.
Result<Com120F8LoadedTestCaseSpectra> prepareCom120F8HistoricalCausalFromSource(
    const SourceSnapshot& source, std::string_view victimChannelId,
    const Com120F8EqualizerChoice& choice,
    const SourceIdentity& observedLiveIdentity,
    double maxAllowedModeConversion = 0.0, Control* control = nullptr);

} // namespace si::channel
