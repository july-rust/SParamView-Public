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
  if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance) throw std::runtime_error(msg);
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
  s.model=IldFitModel::RootLinearQuadratic;
  auto r=calculateIld(fromLoss(exact),s);
  check(r.usable(),"exact polynomial rejected");
  check(r.value->deviationDb.size()==17,"wrong sample count");
  near(r.value->rmsDb,0,1e-8,"exact model RMS");
  near(r.value->coefficients[0],1.5,1e-8,"a0 incorrect");
  near(r.value->coefficients[1],2,1e-8,"root coefficient incorrect");
  near(r.value->coefficients[2],3,1e-8,"linear coefficient incorrect");
  near(r.value->coefficients[3],0.25,1e-8,"quadratic coefficient incorrect");
  s.model=IldFitModel::RootLinear;
  std::vector<double> root;
  for(int i=0;i<=16;++i) { double x=double(i)/16;root.push_back(1+2*std::sqrt(x)+3*x); }
  r=calculateIld(fromLoss(root),s);
  near(r.value->rmsDb,0,1e-9,"root-linear oracle");
  near(r.value->coefficients[0],1,1e-9,"root intercept");
  near(r.value->coefficients[1],2,1e-9,"root coefficient");
  near(r.value->coefficients[2],3,1e-9,"root slope");
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
  // Closed-form weighted linear regression (endpoints have half weight),
  // and exact squared piecewise-linear residual integral.
  near(r.value->coefficients[0],2.0372093023255814,1e-10,"weighted ripple intercept");
  near(r.value->coefficients[1],2.925581395348837,1e-10,"weighted ripple slope");
  near(r.value->rmsDb,0.1134222707483529,1e-10,"band ripple RMS golden");
  near(r.value->peakAbsoluteDb,0.2232558139534886,1e-10,"weighted ripple peak golden");
  // Nonuniform independently calculated weighted straight-line regression.
  const std::vector<double> x{0,0.1,0.3,0.7,1.0}, y{1,3,2,5,4};
  const std::vector<double> weights{0.05,0.15,0.30,0.35,0.15};
  auto irregular=fromLoss(y);
  double sx=0,sy=0,sxx=0,sxy=0;
  for(size_t i=0;i<x.size();++i) {
    irregular.frequencyHz[i]=x[i]*16e9;
    sx+=weights[i]*x[i];sy+=weights[i]*y[i];
    sxx+=weights[i]*x[i]*x[i];sxy+=weights[i]*x[i]*y[i];
  }
  const double slope=(sxy-sx*sy)/(sxx-sx*sx), intercept=sy-slope*sx;
  r=calculateIld(irregular,s);
  near(r.value->coefficients[0],intercept,1e-10,"irregular weighted intercept");
  near(r.value->coefficients[1],slope,1e-10,"irregular weighted slope");
  double band=0,peak=0,previous=0;size_t peakIndex=0;
  for(size_t i=0;i<x.size();++i) {
    const double fitted=intercept+slope*x[i], residual=y[i]-fitted;
    near(r.value->insertionLossDb[i],y[i],1e-10,"irregular loss");
    near(r.value->fittedLossDb[i],fitted,1e-10,"irregular fit");
    near(r.value->deviationDb[i],residual,1e-10,"irregular signed residual");
    if(i) band+=(x[i]-x[i-1])*(previous*previous+previous*residual+residual*residual)/3;
    if(std::abs(residual)>peak){peak=std::abs(residual);peakIndex=i;}
    previous=residual;
  }
  near(r.value->rmsDb,std::sqrt(band),1e-10,"irregular band RMS");
  near(r.value->peakAbsoluteDb,peak,1e-10,"irregular peak");
  near(r.value->peakFrequencyHz,irregular.frequencyHz[peakIndex],1e-5,"irregular peak frequency");
  auto flat=fromLoss({0,0,0,0,0});
  for(auto &frequency:flat.frequencyHz) frequency+=1e9;
  r=calculateIld(flat,s);
  near(r.value->rmsDb,0,1e-12,"flat zero loss");
  near(r.value->peakFrequencyHz,1e9,1e-5,"zero residual first measured peak");
  auto gain=fromLoss({-1,-1,-1,-1,-1});
  r=calculateIld(gain,s);
  near(r.value->insertionLossDb[0],-1,1e-10,"negative insertion loss not clipped");
  auto invalid=irregular;invalid.frequencyHz[1]=invalid.frequencyHz[0];
  reject([&]{calculateIld(invalid,s);},"duplicate frequency");
  invalid=irregular;invalid.transfer[1]={std::numeric_limits<double>::infinity(),0};
  reject([&]{calculateIld(invalid,s);},"infinite transfer");
  reject([&]{calculateIld(ComplexTrace{},s);},"empty input");
  auto narrow=flat;
  for(size_t i=0;i<narrow.frequencyHz.size();++i)narrow.frequencyHz[i]=1e9+double(i)*1e-3;
  reject([&]{calculateIld(narrow,s);},"ill-conditioned fit");
  std::cout<<"012_44 independent weighted and band RMS oracles PASS\n";
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
