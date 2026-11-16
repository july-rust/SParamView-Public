#pragma once
// 012_09: independent, reciprocal two-port circuit primitives for a future COM
// package cascade. These are physical circuit identities, NOT IEEE COM margin.
// They do not infer a package from Sdd21 alone or claim IEEE 120F conformance.
#include "si/channel_analysis/common.hpp"
#include <complex>

namespace si::channel {

struct ComTwoPort {
  // Voltage/current ABCD convention:
  // [V1; I1] = [A B; C D] [V2; -I2] for currents entering both ports.
  Complex a{1.0, 0.0}, b{0.0, 0.0};
  Complex c{0.0, 0.0}, d{1.0, 0.0};
};

struct ComTwoPortS {
  Complex s11{0.0, 0.0}, s21{1.0, 0.0};
  Complex s12{1.0, 0.0}, s22{0.0, 0.0};
};

// Ordered cascade: physically left element followed by right element.
ComTwoPort cascadeTwoPort(const ComTwoPort &left, const ComTwoPort &right);
ComTwoPort seriesImpedance(Complex impedanceOhm);
ComTwoPort shuntAdmittance(Complex admittanceSiemens);
// Standard lossy uniform line ABCD with gamma=alpha+j*2*pi*f*delayPerMm,
// alpha in Np/mm, length in mm, characteristic impedance in Ohm.
// A frequency-independent alpha is a diagnostic model, not a 120F formula.
ComTwoPort lossyUniformLine(double frequencyHz, double lengthMm,
                           double attenuationNpPerMm,
                           double delaySecondsPerMm,
                           double characteristicOhm);
// Matched real Zref at both ends, power-wave scattering conversion.
// Preserves phase, reflection and both directions.
ComTwoPortS twoPortScattering(const ComTwoPort &network,
                             double referenceOhm);

} // namespace si::channel
