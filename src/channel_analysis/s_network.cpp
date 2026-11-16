#include "si/channel_analysis/s_network.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>

namespace si::channel {
namespace {
void require(bool condition, const char *reason) {
  if (!condition) throw Error(reason);
}
bool finite(Complex x) {
  return std::isfinite(x.real()) && std::isfinite(x.imag());
}
void checkZ(double z) {
  require(std::isfinite(z) && z>0, "Invalid wave reference impedance");
}
void checkPassiveTwoPort(const ComTwoPortS &s) {
  require(finite(s.s11) && finite(s.s21) && finite(s.s12) &&
          finite(s.s22), "Nonfinite wave two-port coefficient");
  // Exact 2x2 largest eigenvalue of S^H S is the squared max singular value.
  const double g11 = std::norm(s.s11)+std::norm(s.s21);
  const double g22 = std::norm(s.s12)+std::norm(s.s22);
  const double cross=std::abs(std::conj(s.s11)*s.s12+
                              std::conj(s.s21)*s.s22);
  const double maximum=(g11+g22)*0.5+
      std::hypot((g11-g22)*0.5,cross);
  require(std::isfinite(maximum) && maximum<=1.+2.e-6,
          "Active/nonpassive wave two-port is unsupported");
}
void checkPassiveFourPort(const std::array<Complex,16> &s) {
  for (Complex x:s)
    require(finite(x), "Nonfinite physical four-port S-parameter");
  // Cholesky of (1+epsilon)I-S^H*S. This tests all excited wave
  // combinations, unlike individual |Sij| or column power checks.
  constexpr double safeMargin=2e-6;
  std::array<std::array<Complex,4>,4> gram{};
  for(size_t i=0;i<4;++i)
    for(size_t j=0;j<4;++j) {
      Complex power{0,0};
      for(size_t k=0;k<4;++k)
        power+=std::conj(s[k*4+i])*s[k*4+j];
      gram[i][j]=(i==j?Complex{1+safeMargin,0}:Complex{0,0})-power;
      require(finite(gram[i][j]), "Physical four-port power overflow");
    }
  std::array<std::array<Complex,4>,4> lower{};
  for(size_t i=0;i<4;++i) {
    for(size_t j=0;j<=i;++j) {
      Complex value=gram[i][j];
      for(size_t k=0;k<j;++k)
        value-=lower[i][k]*std::conj(lower[j][k]);
      if(i==j) {
        require(std::isfinite(value.real()) && value.real()>1e-13 &&
                std::abs(value.imag())<=1e-8,
                "Active/nonpassive physical four-port input");
        lower[i][j]={std::sqrt(value.real()),0};
      } else {
        require(std::abs(lower[j][j])>0,
                "Singular four-port passivity factorization");
        lower[i][j]=value/lower[j][j];
      }
    }
  }
}
}
ComWaveTwoPort toComWave(const ComTwoPort &network,
                        double referenceOhm) {
  checkZ(referenceOhm);
  auto s=twoPortScattering(network,referenceOhm);
  checkPassiveTwoPort(s);
  return {s,referenceOhm};
}
ComWaveTwoPort cascadeComScattering(const ComWaveTwoPort &left,
                                     const ComWaveTwoPort &right) {
  checkZ(left.referenceOhm); checkZ(right.referenceOhm);
  require(std::abs(left.referenceOhm-right.referenceOhm)<=
           1e-12*std::max(left.referenceOhm,right.referenceOhm),
          "Wave cascade port reference impedances differ");
  const auto &a=left.s, &b=right.s;
  checkPassiveTwoPort(a); checkPassiveTwoPort(b);
  const Complex denominator=1.-a.s22*b.s11;
  require(finite(denominator) && std::abs(denominator)>=1e-12,
          "Singular internal wave reflection loop");
  ComTwoPortS result;
  result.s11=a.s11+(a.s12*b.s11*a.s21)/denominator;
  result.s12=(a.s12*b.s12)/denominator;
  result.s21=(b.s21*a.s21)/denominator;
  result.s22=b.s22+(b.s21*a.s22*b.s12)/denominator;
  checkPassiveTwoPort(result);
  return {result,left.referenceOhm};
}
ComDifferentialExtraction extractDifferentialComNetwork(
    const ComPhysicalFourPort &physical,
    double maximumAllowedModeConversion) {
  require(std::isfinite(maximumAllowedModeConversion) &&
          maximumAllowedModeConversion>=0 &&
          maximumAllowedModeConversion<=1,
          "Invalid mixed-mode conversion acceptance limit");
  checkZ(physical.referenceOhm[0]);
  for(double z:physical.referenceOhm) {
    checkZ(z);
    require(std::abs(z-physical.referenceOhm[0])<=
                1e-12*physical.referenceOhm[0],
            "Four-port equal single-ended reference impedances required");
  }
  checkPassiveFourPort(physical.scattering);
  constexpr double invSqrt2=0.7071067811865475244;
  // Input physical order nP,nN,fP,fN -> dNear,cNear,dFar,cFar.
  constexpr double h[4][4] = {
      {invSqrt2,-invSqrt2,0,0},
      {invSqrt2, invSqrt2,0,0},
      {0,0,invSqrt2,-invSqrt2},
      {0,0,invSqrt2, invSqrt2}
  };
  std::array<std::array<Complex,4>,4> mixed{};
  for(size_t i=0;i<4;++i)
    for(size_t j=0;j<4;++j)
      for(size_t a=0;a<4;++a)
        for(size_t b=0;b<4;++b)
          mixed[i][j]+=h[i][a]*physical.scattering[a*4+b]*h[j][b];
  double worst=0;
  for(size_t i=0;i<4;++i)
    for(size_t j=0;j<4;++j)
      if((i%2)!=(j%2)) worst=std::max(worst,std::abs(mixed[i][j]));
  require(std::isfinite(worst) &&
          worst<=maximumAllowedModeConversion+1e-12,
          "Measured mode conversion exceeds declared COM approximation limit");
  ComTwoPortS d;
  d.s11=mixed[0][0];
  d.s21=mixed[2][0];
  d.s12=mixed[0][2];
  d.s22=mixed[2][2];
  checkPassiveTwoPort(d);
  const double differentialZ=2*physical.referenceOhm[0];
  checkZ(differentialZ);
  return {{d,differentialZ},worst};
}
} // namespace si::channel
