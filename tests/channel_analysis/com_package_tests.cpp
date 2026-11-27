#include "si/channel_analysis/package.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace si;
using namespace si::channel;
namespace {
void require(bool p,const char *message) {
  if(!p)throw std::runtime_error(message);
}
void close(Complex actual,Complex expected,double eps,const char *what) {
  if(!std::isfinite(actual.real()) || !std::isfinite(actual.imag()) ||
     std::abs(actual-expected)>eps) throw std::runtime_error(what);
}
template<class Fn>void reject(Fn f,const char *why) {
  bool threw=false;
  try{f();}catch(const Error&){threw=true;}
  require(threw,why);
}
ComPackageCircuit example() {
  ComPackageCircuit p;
  p.primaryLineSingleEndedOhm=50;
  p.secondaryLineSingleEndedOhm=50;
  return p;
}
}
int main() {
  try{
    const double f=10e9;
    auto p=example();
    const auto ideal=calculateBalancedComPackage(
        p,f,ComPackageSide::TxDieToBoard);
    require(ideal.referenceOhm==100,"SE 50 -> diff 100-ohm");
    close(ideal.s.s21,{1,0},1e-14,"zero-element identity");
    close(ideal.s.s11,{0,0},1e-14,"zero-element no reflection");
    p.cdFarad=100e-15;
    const auto cap=calculateBalancedComPackage(
        p,f,ComPackageSide::TxDieToBoard);
    // Differential Y=j*2*pi*f*(Cd/2); S21=2/(2+Y*Zref).
    const Complex equivalent{0,2*std::acos(-1.)*f*p.cdFarad/2};
    close(cap.s.s21,Complex{2,0}/(Complex{2,0}+equivalent*100.),
          1e-13,"single-ended capacitance to differential conversion");
    p=example();p.seriesInductanceHenry=120e-12;
    const auto inductor=calculateBalancedComPackage(
        p,f,ComPackageSide::TxDieToBoard);
    // Differential series impedance = j*omega*(2*single-ended Ls).
    const Complex series{0,2*std::acos(-1.)*f*(2*p.seriesInductanceHenry)};
    close(inductor.s.s21,Complex{200,0}/(Complex{200,0}+series),
          1e-13,"single-ended inductance to differential conversion");
    p=example();
    p.primaryLineMm=13; p.secondaryLineMm=1.8;
    p.attenuationSqrtGHzPerMm=9.909e-4;
    p.attenuationGHzPerMm=2.772e-4;
    p.delaySecondsPerMm=6e-12; // explicit *test* parameter, not normative
    const double alpha=p.attenuationSqrtGHzPerMm*std::sqrt(f/1e9)+
                       p.attenuationGHzPerMm*(f/1e9);
    const auto transmission=calculateBalancedComPackage(
        p,f,ComPackageSide::TxDieToBoard);
    close(transmission.s.s21,
          std::exp(Complex{-alpha*(13+1.8),
                            -2*std::acos(-1.)*f*6e-12*(13+1.8)}),
          1e-12,"frequency-dependent package gamma analytic oracle");
    close(transmission.s.s11,{0,0},1e-12,
          "matched two-line package zero reflection");
    const auto lengths1=com8023ckC2cPackageLengths(ComPackageCase::Test1);
    const auto lengths2=com8023ckC2cPackageLengths(ComPackageCase::Test2);
    require(lengths2.txMm>lengths1.txMm &&
            lengths2.rxMm>lengths1.rxMm,
            "different package cases must produce different physical lengths");
    p.primaryLineMm=lengths2.txMm;
    const auto longer=calculateBalancedComPackage(
        p,f,ComPackageSide::TxDieToBoard);
    require(std::abs(longer.s.s21)<std::abs(transmission.s.s21),
            "longer package must lose more in passive model");
    p=example();p.cdFarad=100e-15;p.cbFarad=30e-15;
    p.cpFarad=80e-15;p.seriesInductanceHenry=100e-12;
    p.primaryLineMm=13;p.secondaryLineMm=1.8;
    p.primaryLineSingleEndedOhm=87.5;
    p.secondaryLineSingleEndedOhm=92.5;
    const auto tx=calculateBalancedComPackage(
        p,f,ComPackageSide::TxDieToBoard);
    const auto rx=calculateBalancedComPackage(
        p,f,ComPackageSide::RxBoardToDie);
    close(tx.s.s21,rx.s.s21,1e-12,
          "reversing order preserves passive reciprocal transmission");
    close(tx.s.s11,rx.s.s22,1e-12,
          "reversing network interchanges input/output reflection");
    close(tx.s.s22,rx.s.s11,1e-12,
          "reversing network interchanges output/input reflection");
    reject([&]{(void)calculateBalancedComPackage(
        p,-1,ComPackageSide::TxDieToBoard);},"negative frequency");
    auto wrong=p;wrong.cdFarad=NAN;
    reject([&]{(void)calculateBalancedComPackage(
        wrong,f,ComPackageSide::TxDieToBoard);},"missing capacitance");
    wrong=p;wrong.primaryLineSingleEndedOhm=0;
    reject([&]{(void)calculateBalancedComPackage(
        wrong,f,ComPackageSide::TxDieToBoard);},"missing Zc");
    wrong=p;wrong.attenuationGHzPerMm=-1;
    reject([&]{(void)calculateBalancedComPackage(
        wrong,f,ComPackageSide::TxDieToBoard);},"active loss");
    reject([&]{(void)calculateBalancedComPackage(
        p,f,static_cast<ComPackageSide>(37));},"invalid orientation");
    std::cout<<"012_09 balanced device package circuit analytic oracles PASS; "
                "not IEEE 802.3ck COM margin\n";
    return 0;
  }catch(const std::exception &e){
    std::cerr<<"012_09 balanced device package FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
