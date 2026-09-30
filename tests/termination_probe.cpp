#include "si/core.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char **argv){try{
 if(argc!=6)throw si::Error("source mapping.csv cache-dir output-dir reverse");
 auto cache=si::Cache::open(si::pathFromUtf8(argv[1]),si::pathFromUtf8(argv[3]));auto channels=si::readMappingCsv(si::pathFromUtf8(argv[2]));si::validateMapping(channels,cache->meta());
 auto dir=si::pathFromUtf8(argv[4]);si::fs::create_directories(dir);si::Settings set;set.reverse=std::stoi(argv[5])!=0;set.tdrStopSeconds=3e-9;
 set.tdrTerminations={si::Termination::Reference,si::Termination::Resistor,si::Termination::Split,si::Termination::Open,si::Termination::Short};
 std::vector<si::Job> jobs;for(auto &c:channels)jobs.push_back({cache,c,std::nullopt,si::Metric::TDR,"Validation"});jobs=si::expandTdrJobs(jobs,set);
 std::ofstream index(dir/"index.csv");index<<"index,channel,termination,quality,nf,nt,reference,note\n";
 for(size_t i=0;i<jobs.size();++i){auto &j=jobs[i];auto t=si::loadTrace(*cache,j.victim,j.metric,set,nullptr,nullptr,j.termination);auto td=si::transformTdr(t,set);
  std::ofstream out(dir/(std::to_string(i)+".bin"),std::ios::binary);uint64_t nf=t.x.size(),nt=td.time.size();out.write((char*)&nf,8);out.write((char*)t.x.data(),nf*8);out.write((char*)t.s.data(),nf*16);out.write((char*)&nt,8);out.write((char*)td.time.data(),nt*8);out.write((char*)td.impedance.data(),nt*8);out.write((char*)td.reflection.data(),nt*8);if(!out)throw si::Error("write failed");
  index<<i<<','<<j.victim.name<<','<<int(j.termination)<<','<<td.quality<<','<<nf<<','<<nt<<','<<t.referenceOhm<<",\""<<td.note<<"\"\n";
  if(td.time.empty())throw si::Error(td.note);
 }std::cout<<jobs.size()<<" conditions\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
