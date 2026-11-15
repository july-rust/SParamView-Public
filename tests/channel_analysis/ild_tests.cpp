#include "si/channel_analysis/ild.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace si;
using namespace si::channel;
namespace {
void check(bool ok,const char *msg){if(!ok) throw std::runtime_error(msg);}
void near(double a,double b,double tolerance,const char *msg){
  if(std::abs(a-b)>tolerance) throw std::runtime_error(msg);
}
ComplexTrace fromLoss(const std::vector<double> &db){
  ComplexTrace t;
  t.stimulusOhm=50;t.responseOhm=50;t.parameter="S21";
  for(size_t i=0;i<db.size();++i) {
    t.frequencyHz.push_back(double(i)*1e9);
    // Independent oracle: |S21| = 10^(-IL/20).
    t.transfer.emplace_back(std::pow(10.0,-db[i]/20.0),0);
  }
  return t;
}
template<class F> void reject(F fn,const char *message) {
  bool caught=false;try{fn();}catch(const Error&){caught=true;}
  check(caught,message);
}
}
int main() {
 try {
  std::vector<double> exact, linear;
  for(int i=0;i<=16;++i) {
    double x=double(i)/16.0;
    exact.push_back(1.5+2.0*std::sqrt(x)+3.0*x+0.25*x*x);
    linear.push_back(2.0+3.0*x);
  }
  IldSettings s; s.startHz=0;s.stopHz=16e9;
  auto r=calculateIld(fromLoss(exact),s);
  check(r.usable(),"exact polynomial rejected");
  check(r.value->deviationDb.size()==17,"wrong sample count");
  near(r.value->rmsDb,0,1e-8,"exact model RMS");
  near(r.value->coefficients[0],1.5,1e-8,"a0 incorrect");
  near(r.value->coefficients[1],2,1e-8,"root coefficient incorrect");
  near(r.value->coefficients[2],3,1e-8,"linear coefficient incorrect");
  near(r.value->coefficients[3],0.25,1e-8,"quadratic coefficient incorrect");
  s.model=IldFitModel::Linear;
  r=calculateIld(fromLoss(linear),s);
  near(r.value->rmsDb,0,1e-9,"linear oracle");
  near(r.value->coefficients[0],2,1e-9,"linear constant");
  near(r.value->coefficients[1],3,1e-9,"linear slope");
  std::vector<double> ripple=linear;
  for(size_t i=0;i<ripple.size();++i)
    ripple[i]+=0.2*std::sin(2*3.141592653589793*double(i)/4.0);
  r=calculateIld(fromLoss(ripple),s);
  check(r.value->rmsDb>0.05 && r.value->peakAbsoluteDb>0.1,
        "ripple deliberately disturbed");
  check(r.value->minimumDb<0 && r.value->maximumDb>0,"signed ripple");
  // Independent closed-form linear regression for a 17-point 0.2 dB
  // quarter-period ripple: residual sum of squares = 16/51 dB^2.
  near(r.value->rmsDb,std::sqrt(16.0/867.0),1e-10,"independent ripple RMS golden");
  near(r.value->peakAbsoluteDb,0.2196078431372549,1e-10,
       "independent ripple peak golden");
  s.stopHz=8e9;
  r=calculateIld(fromLoss(linear),s);
  near(r.value->rangeStopHz,8e9,1e-5,"range cap");
  check(r.value->frequencyHz.size()==9,"frequency filtering");
  s.startHz=12e9;
  reject([&]{calculateIld(fromLoss(linear),s);},"reversed range");
  s.startHz=0;s.stopHz=16e9;
  auto zero=fromLoss(linear);zero.transfer[7]={0,0};
  reject([&]{calculateIld(zero,s);},"zero transfer");
  auto nan=fromLoss(linear);
  nan.transfer[4]={std::numeric_limits<double>::quiet_NaN(),0};
  reject([&]{calculateIld(nan,s);},"NaN");
  auto notEnough=fromLoss({1,2,3,4});
  s.model=IldFitModel::RootLinearQuadratic;
  reject([&]{calculateIld(notEnough,s);},"insufficient fit samples");
  std::atomic_bool cancelled{true};
  reject([&]{calculateIld(fromLoss(linear),s,{},&cancelled);},"cancellation");
  std::cout<<"012_05 ILD numeric oracle PASS\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<"ILD FAIL: "<<e.what()<<'\n';return 1;}
}
