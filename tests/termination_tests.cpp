#include "si/core.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace si;
int checks=0;
void expect(bool v,const char *m){++checks;if(!v)throw Error(m);}
void near(Complex a,Complex b){expect(std::abs(a-b)<2e-12,"Analytic load mismatch");}
int main(){
 auto dir=fs::temp_directory_path()/"si_termination_tests";fs::create_directories(dir);
 try{
 Channel ch;ch.id=ch.name="scrambled";ch.nearP=2;ch.nearN=0;ch.farP=3;ch.farN=1;
 for(double ref:{50.,75.})for(bool unbalanced:{false,true}){
  auto path=dir/(std::to_string(int(ref))+(unbalanced?"-u.s4p":"-b.s4p"));
  std::ofstream out(path);out<<std::setprecision(17)<<"# Hz S RI R "<<ref<<'\n';
  for(int k=0;k<=256;++k){
   double f=k*40e6;Complex s[4][4]{};
   Complex a=std::polar(.9,-2*3.14159265358979323846*f*.2e-9),b=unbalanced?std::polar(.72,-2*3.14159265358979323846*f*.25e-9):a;
   s[3][2]=a;s[2][3]=a*.8;s[1][0]=b;s[0][1]=b*.8;
   out<<f<<' ';for(auto &row:s)for(auto z:row)out<<z.real()<<' '<<z.imag()<<' ';out<<'\n';
  }out.close();auto cache=Cache::open(path,dir/"cache");validateMapping({ch},cache->meta());
  for(bool reverse:{false,true})for(int term=0;term<5;++term){
   Settings set;set.reverse=reverse;set.tdrStopSeconds=3e-9;
   auto t=loadTrace(*cache,ch,Metric::TDR,set,nullptr,nullptr,Termination(term));
   for(size_t k=0;k<t.x.size();++k){
    double f=t.x[k];Complex a=std::polar(.9,-2*3.14159265358979323846*f*.2e-9),b=unbalanced?std::polar(.72,-2*3.14159265358979323846*f*.25e-9):a,expected{};
    if(term==1)expected=.8*((1-2*ref/(100+2*ref))*(a*a+b*b)/2.-2*ref/(100+2*ref)*a*b);
    if(term==2)expected=.8*(50-ref)/(50+ref)*(a*a+b*b)/2.;
    if(term==3)expected=.8*(a*a+b*b)/2.;
    if(term==4)expected=-.8*a*b;
    near(t.s[k],expected);
   }
   set.tdrReflection=true;set.tdrLimitEnabled=true;auto r=analyze(t,Metric::TDR,set);
   expect(r.status=="N/A","rho has no ohm limits");expect(r.termination==Termination(term),"result load");expect(r.plotX.size()>10,"TDR samples");
   auto td=transformTdr(t,set);expect(td.time.size()==td.reflection.size(),"rho size");expect(crop(t,1e7,1e9).termination==t.termination,"crop load");
  }
  Channel se=ch;se.nearN=se.farN=-1;Settings set;
  for(int term=0;term<5;++term){
   auto t=loadTrace(*cache,se,Metric::TDR,set,nullptr,nullptr,Termination(term));
   double g=term==0?0:(term==1||term==2)?(50-ref)/(50+ref):term==3?1:-1;
   for(size_t k=0;k<t.x.size();++k)near(t.s[k],.8*g*std::polar(.81,-4*3.14159265358979323846*t.x[k]*.2e-9));
  }
  set.tdrTerminations={Termination::Resistor,Termination::Split,Termination::Open};
  std::vector<Job> jobs{{cache,ch,std::nullopt,Metric::TDR,"r"},{cache,se,std::nullopt,Metric::TDR,"r"},{cache,ch,std::nullopt,Metric::RL,"r"}};
  auto expanded=expandTdrJobs(jobs,set);expect(expanded.size()==6,"SE deduplicate, RL unchanged");set.tdrStopSeconds=3e-9;expect(runJobs(expanded,set).size()==6,"runner count");
  Control c;c.cancelled=true;bool caught=false;try{loadTrace(*cache,ch,Metric::TDR,set,nullptr,&c,Termination::Open);}catch(const Cancelled &){caught=true;}expect(caught,"cancellation");
  auto missing=ch;missing.farP=missing.farN=-1;caught=false;try{loadTrace(*cache,missing,Metric::TDR,set,nullptr,nullptr,Termination::Resistor);}catch(const Error &){caught=true;}expect(caught,"missing mapping");
  set.tdrTerminations={Termination(99)};caught=false;try{expandTdrJobs(jobs,set);}catch(const Error &){caught=true;}expect(caught,"invalid enum");
 }
 auto path=dir/"isolated.s4p";std::ofstream out(path);out<<"# Hz S RI R 50\n";
 for(int k=0;k<16;++k){double s[4][4]{};for(int i:{3,1})for(int j:{3,1})s[i][j]=.5;out<<k*1e8<<' ';for(auto &row:s)for(auto z:row)out<<z<<" 0 ";out<<'\n';}out.close();
 auto cache=Cache::open(path,dir/"cache");Settings set;
 for(auto term:{Termination::Resistor,Termination::Short,Termination::Open})for(auto z:loadTrace(*cache,ch,Metric::TDR,set,nullptr,nullptr,term).s)near(z,0);
 std::cout<<"PASS "<<checks<<" termination checks\n";return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
