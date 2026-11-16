#pragma once
// 012_09: independent, reciprocal two-port circuit primitives for a future COM
// package cascade. These are physical circuit identities, NOT IEEE COM margin.
// They do not infer a package from Sdd21 alone or claim IEEE 120F conformance.
#include "si/channel_analysis/common.hpp"
#include <complex>

namespace si::channel {

// Final 802.3ck-2022 120F.4.1 identifies *two distinct* package length
// configurations. Values alone do not qualify the full Table 120F-8 model.
enum class ComPackageCase { Test1, Test2 };
struct ComPackageLengths {
  double txMm = 0, rxMm = 0;
};
ComPackageLengths com8023ckC2cPackageLengths(ComPackageCase test);

struct ComTwoPortS;
// Closed, reciprocal-only ABCD: values can be constructed only through the
// analytically reciprocal series/shunt/line and cascade operations. No public
// mutable matrix coefficients or caller-supplied nonreciprocal matrices.
class ComTwoPort final {
public:
  ComTwoPort() = default; // identity
  Complex a() const { return a_; }
  Complex b() const { return b_; }
  Complex c() const { return c_; }
  Complex d() const { return d_; }
private:
  // Voltage/current convention:
  // [V1; I1] = [A B; C D] [V2; -I2], I1/I2 enter each port.
  Complex a_{1.0, 0.0}, b_{0.0, 0.0};
  Complex c_{0.0, 0.0}, d_{1.0, 0.0};
  ComTwoPort(Complex a, Complex b, Complex c, Complex d)
    : a_(a), b_(b), c_(c), d_(d) {}
  friend ComTwoPort cascadeTwoPort(const ComTwoPort &, const ComTwoPort &);
  friend ComTwoPort seriesImpedance(Complex);
  friend ComTwoPort shuntAdmittance(Complex);
  friend ComTwoPort lossyUniformLine(double, double, double, double, double);
  friend ComTwoPortS twoPortScattering(const ComTwoPort &, double);
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
