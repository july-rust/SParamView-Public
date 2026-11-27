#include "si/channel_analysis/com_causal_package.hpp"
#include <cmath>
#include <numbers>

namespace si::channel {
namespace {
void require(bool yes, const char *message) {
  if (!yes) throw Error(message);
}
void nonnegative(double x, const char *message) {
  require(std::isfinite(x) && x>=0,message);
}
void positive(double x, const char *message) {
  require(std::isfinite(x) && x>0,message);
}
void validate(const ComCausalPackageParameters &p) {
  for (double x:{p.cdSingleEndedFarad,p.lsSingleEndedHenry,
                 p.cbSingleEndedFarad,p.cpSingleEndedFarad,
                 p.primaryLengthMm,p.secondaryLengthMm,
                 p.gamma0PerMm,p.a1SqrtNsPerMm,p.a2NsPerMm,
                 p.tauNsPerMm})
    nonnegative(x,"Invalid causal package geometry or parameter");
  positive(p.primaryZcDifferentialOhm,
           "Missing primary DIFFERENTIAL package characteristic ohms");
  positive(p.secondaryZcDifferentialOhm,
           "Missing secondary DIFFERENTIAL package characteristic ohms");
}
}
Complex comCausalGammaPerMm(const ComCausalPackageParameters &p,
                             double frequencyHz) {
  validate(p);
  nonnegative(frequencyHz,"Invalid causal package frequency");
  if (frequencyHz==0) return {p.gamma0PerMm,0.0};
  const double fGHz=frequencyHz/1e9;
  positive(fGHz,"Underflow in causal package normalized frequency");
  const double rt=std::sqrt(fGHz);
  const double alpha=p.gamma0PerMm+p.a1SqrtNsPerMm*rt+
                       p.a2NsPerMm*fGHz;
  const double beta=p.a1SqrtNsPerMm*rt+
      fGHz*(2*std::numbers::pi*p.tauNsPerMm-
            (2/std::numbers::pi)*p.a2NsPerMm*std::log(fGHz));
  require(std::isfinite(alpha) && std::isfinite(beta) && alpha>=0,
          "Invalid causal complex propagation constant");
  return {alpha,beta};
}
ComWaveTwoPort comCausalLine(const ComCausalPackageParameters &p,
                             double frequencyHz, double lengthMm,
                             double zcDifferentialOhm,
                             double zrefDifferentialOhm) {
  validate(p);
  nonnegative(lengthMm,"Invalid causal line length");
  positive(zcDifferentialOhm,"Invalid DIFFERENTIAL package Zc");
  positive(zrefDifferentialOhm,"Invalid DIFFERENTIAL reference impedance");
  const Complex exponent=-comCausalGammaPerMm(p,frequencyHz)*lengthMm;
  require(std::isfinite(exponent.real()) && exponent.real()<=0 &&
          exponent.real()>=-600 &&
          std::isfinite(exponent.imag()),
          "Causal line propagation out of qualified dynamic range");
  const Complex e=std::exp(exponent);
  const double rho=(zcDifferentialOhm-zrefDifferentialOhm)/
                   (zcDifferentialOhm+zrefDifferentialOhm);
  require(std::isfinite(rho) && std::abs(rho)<1,
          "Unphysical matched-wave causal line reflection");
  const Complex denominator=Complex{1,0}-(rho*rho)*e*e;
  require(std::isfinite(denominator.real()) &&
          std::isfinite(denominator.imag()) &&
          std::abs(denominator)>1e-15,
          "Causal package line singular scattering network");
  const Complex s21=(1-rho*rho)*e/denominator;
  const Complex s11=rho*(Complex{1,0}-e*e)/denominator;
  require(std::isfinite(s21.real()) && std::isfinite(s21.imag()) &&
          std::isfinite(s11.real()) && std::isfinite(s11.imag()),
          "Nonfinite causal line scattering");
  ComWaveTwoPort out;
  out.referenceOhm=zrefDifferentialOhm;
  out.s.s11=out.s.s22=s11;
  out.s.s21=out.s.s12=s21;
  return out;
}
ComWaveTwoPort calculateComCausalPackage(
    const ComCausalPackageParameters &p,
    double frequencyHz, ComCausalPackageSide side,
    double physicalSingleEndedReferenceOhm) {
  validate(p);
  nonnegative(frequencyHz,"Invalid causal package frequency");
  positive(physicalSingleEndedReferenceOhm,
           "Invalid single-ended package reference impedance");
  require(side==ComCausalPackageSide::TxDieToBoard ||
          side==ComCausalPackageSide::RxBoardToDie,
          "Unknown causal package orientation");
  const double zref=2*physicalSingleEndedReferenceOhm;
  positive(zref,"Differential reference impedance overflow");
  const Complex jOmega{0,2*std::numbers::pi*frequencyHz};
  // For two balanced independent legs, Cdiff=Csingle/2 and
  // Ldiff=2*Lsingle. By contrast Zc is already differential!
  const auto cd=toComWave(shuntAdmittance(jOmega*
                         (p.cdSingleEndedFarad/2)),zref);
  const auto ls=toComWave(seriesImpedance(jOmega*
                         (2*p.lsSingleEndedHenry)),zref);
  const auto cb=toComWave(shuntAdmittance(jOmega*
                         (p.cbSingleEndedFarad/2)),zref);
  const auto cp=toComWave(shuntAdmittance(jOmega*
                         (p.cpSingleEndedFarad/2)),zref);
  const auto line1=comCausalLine(p,frequencyHz,p.primaryLengthMm,
                                p.primaryZcDifferentialOhm,zref);
  const auto line2=comCausalLine(p,frequencyHz,p.secondaryLengthMm,
                                p.secondaryZcDifferentialOhm,zref);
  ComWaveTwoPort response; response.referenceOhm=zref;
  if(side==ComCausalPackageSide::TxDieToBoard) {
    for(const auto& element:{cd,ls,cb,line1,line2,cp})
      response=cascadeComScattering(response,element);
  } else {
    for(const auto& element:{cp,line2,line1,cb,ls,cd})
      response=cascadeComScattering(response,element);
  }
  return response;
}
ComCausalPackageParameters historical8023ckC2cCausalCandidate(
    double primaryLengthMm) {
  nonnegative(primaryLengthMm,"Invalid historical C2C package length");
  ComCausalPackageParameters p;
  // Cross-referenced IEEE working-group slides + Ansys 2025R1 public
  // COM 3.40 C2C profile. Does not assert approved-final COM lock.
  p.cdSingleEndedFarad=1.2e-13; // 0.00012 nF
  p.lsSingleEndedHenry=1.2e-10; // 0.12 nH
  p.cbSingleEndedFarad=3e-14;   // 0.00003 nF
  p.cpSingleEndedFarad=8.7e-14; // 0.000087 nF
  p.primaryLengthMm=primaryLengthMm;
  p.secondaryLengthMm=1.8;
  p.primaryZcDifferentialOhm=87.5;
  p.secondaryZcDifferentialOhm=92.5;
  p.gamma0PerMm=0;
  p.a1SqrtNsPerMm=9.909e-4;
  p.a2NsPerMm=2.772e-4;
  p.tauNsPerMm=0.006141;
  validate(p);
  return p;
}
} // namespace si::channel
