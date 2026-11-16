#include "si/channel_analysis/two_port.hpp"
#include <cmath>
#include <limits>
#include <numbers>

namespace si::channel {
namespace {
void require(bool condition, const char *what) {
  if (!condition) throw Error(what);
}
bool finite(Complex value) {
  return std::isfinite(value.real()) && std::isfinite(value.imag());
}
void check(const ComTwoPort &v) {
  require(finite(v.a) && finite(v.b) && finite(v.c) && finite(v.d),
          "Nonfinite COM two-port network");
}
}
ComPackageLengths com8023ckC2cPackageLengths(ComPackageCase test) {
  switch (test) {
    case ComPackageCase::Test1: return {13.0, 11.0};
    case ComPackageCase::Test2: return {31.0, 29.0};
  }
  throw Error("Unsupported IEEE 802.3ck COM package test");
}
ComTwoPort cascadeTwoPort(const ComTwoPort &left,
                          const ComTwoPort &right) {
  check(left); check(right);
  const ComTwoPort r{
    left.a*right.a + left.b*right.c,
    left.a*right.b + left.b*right.d,
    left.c*right.a + left.d*right.c,
    left.c*right.b + left.d*right.d
  };
  check(r);
  return r;
}
ComTwoPort seriesImpedance(Complex impedanceOhm) {
  require(finite(impedanceOhm), "Invalid series impedance");
  // For a passive package the physical caller must choose Re(Z)>=0.
  require(impedanceOhm.real() >= 0, "Negative series resistance");
  return {{1,0}, impedanceOhm, {0,0}, {1,0}};
}
ComTwoPort shuntAdmittance(Complex admittanceSiemens) {
  require(finite(admittanceSiemens), "Invalid shunt admittance");
  require(admittanceSiemens.real() >= 0, "Negative shunt conductance");
  return {{1,0}, {0,0}, admittanceSiemens, {1,0}};
}
ComTwoPort lossyUniformLine(double frequencyHz, double lengthMm,
                           double attenuationNpPerMm,
                           double delaySecondsPerMm,
                           double characteristicOhm) {
  require(std::isfinite(frequencyHz) && frequencyHz >= 0,
          "Invalid line frequency");
  require(std::isfinite(lengthMm) && lengthMm >= 0,
          "Invalid line length");
  require(std::isfinite(attenuationNpPerMm) && attenuationNpPerMm >= 0,
          "Invalid line attenuation");
  require(std::isfinite(delaySecondsPerMm) && delaySecondsPerMm >= 0,
          "Invalid line phase delay");
  require(std::isfinite(characteristicOhm) && characteristicOhm > 0,
          "Invalid line characteristic impedance");
  const double loss=attenuationNpPerMm*lengthMm;
  const double phase=2*std::numbers::pi*frequencyHz*
                     delaySecondsPerMm*lengthMm;
  require(std::isfinite(loss) && loss <= 300 && std::isfinite(phase),
          "Overflow or excessive phase in two-port line");
  const Complex propagation{loss, phase};
  const Complex ac=std::cosh(propagation);
  const Complex sh=std::sinh(propagation);
  const ComTwoPort result{
    ac, characteristicOhm*sh, sh/characteristicOhm, ac
  };
  check(result);
  return result;
}
ComTwoPortS twoPortScattering(const ComTwoPort &network,
                             double referenceOhm) {
  check(network);
  require(std::isfinite(referenceOhm) && referenceOhm > 0,
          "Invalid scattering reference impedance");
  const Complex bz = network.b / referenceOhm;
  const Complex cz = network.c * referenceOhm;
  const Complex denominator=network.a+bz+cz+network.d;
  require(finite(denominator) && std::abs(denominator) >= 1e-180,
          "Singular COM two-port conversion");
  const Complex determinant=network.a*network.d -
                            network.b*network.c;
  require(finite(determinant), "Nonfinite two-port determinant");
  ComTwoPortS out;
  out.s11=(network.a+bz-cz-network.d)/denominator;
  out.s21=Complex{2,0}/denominator;
  out.s12=Complex{2,0}*determinant/denominator;
  out.s22=(-network.a+bz-cz+network.d)/denominator;
  require(finite(out.s11) && finite(out.s21) &&
          finite(out.s12) && finite(out.s22),
          "Nonfinite converted two-port scattering parameter");
  return out;
}
} // namespace si::channel
