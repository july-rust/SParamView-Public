#include "si/channel_analysis/package.hpp"
#include <cmath>
#include <numbers>

namespace si::channel {
namespace {
void require(bool valid, const char *why) {
  if (!valid) throw Error(why);
}
void checkNonnegative(double value, const char *why) {
  require(std::isfinite(value) && value>=0,why);
}
void checkPositive(double value, const char *why) {
  require(std::isfinite(value) && value>0,why);
}
}
ComWaveTwoPort calculateBalancedComPackage(
    const ComPackageCircuit &c, double frequencyHz,
    ComPackageSide side, double physicalReferenceOhm) {
  require(std::isfinite(frequencyHz) && frequencyHz>=0,
          "Invalid package analysis frequency");
  checkPositive(physicalReferenceOhm, "Invalid physical package Zref");
  checkNonnegative(c.cdFarad, "Invalid Cd capacitor");
  checkNonnegative(c.cbFarad, "Invalid Cb capacitor");
  checkNonnegative(c.cpFarad, "Invalid Cp capacitor");
  checkNonnegative(c.seriesInductanceHenry, "Invalid Ls series inductor");
  checkNonnegative(c.primaryLineMm, "Invalid primary package length");
  checkNonnegative(c.secondaryLineMm, "Invalid secondary package length");
  checkPositive(c.primaryLineSingleEndedOhm, "Invalid primary package Zc");
  checkPositive(c.secondaryLineSingleEndedOhm, "Invalid secondary package Zc");
  checkNonnegative(c.attenuationSqrtGHzPerMm, "Invalid sqrt-GHz attenuation");
  checkNonnegative(c.attenuationGHzPerMm, "Invalid linear-GHz attenuation");
  checkNonnegative(c.delaySecondsPerMm, "Invalid package delay");
  require(side==ComPackageSide::TxDieToBoard ||
          side==ComPackageSide::RxBoardToDie,
          "Unsupported package orientation");
  const double differentialReferenceOhm=2.*physicalReferenceOhm;
  checkPositive(differentialReferenceOhm,
                "Invalid differential package Zref");
  const double fGHz=frequencyHz/1e9;
  const double alpha=c.attenuationSqrtGHzPerMm*std::sqrt(fGHz)+
                     c.attenuationGHzPerMm*fGHz;
  checkNonnegative(alpha, "Package frequency-dependent loss overflow");
  const Complex jOmega{0,2*std::numbers::pi*frequencyHz};
  // Physical differential equivalent of two symmetric independent legs.
  const auto cd=shuntAdmittance(jOmega*(c.cdFarad/2.));
  const auto ls=seriesImpedance(jOmega*(c.seriesInductanceHenry*2.));
  const auto cb=shuntAdmittance(jOmega*(c.cbFarad/2.));
  const auto cp=shuntAdmittance(jOmega*(c.cpFarad/2.));
  const auto line1=lossyUniformLine(frequencyHz,c.primaryLineMm,alpha,
                                  c.delaySecondsPerMm,
                                  c.primaryLineSingleEndedOhm*2.);
  const auto line2=lossyUniformLine(frequencyHz,c.secondaryLineMm,alpha,
                                  c.delaySecondsPerMm,
                                  c.secondaryLineSingleEndedOhm*2.);
  ComTwoPort result;
  if(side==ComPackageSide::TxDieToBoard) {
    for(const ComTwoPort &part:{cd,ls,cb,line1,line2,cp})
      result=cascadeTwoPort(result,part);
  } else {
    for(const ComTwoPort &part:{cp,line2,line1,cb,ls,cd})
      result=cascadeTwoPort(result,part);
  }
  return toComWave(result,differentialReferenceOhm);
}
} // namespace si::channel
