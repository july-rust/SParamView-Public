#include "si/core.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <functional>
#include <thread>
using namespace si;
static void require(bool condition,const char *message){if(!condition)throw Error(message);}
static fs::path dir;
static fs::path write(const std::string &name,const std::string &body){auto p=dir/name;std::ofstream f(p,std::ios::binary);f<<body;f.close();return p;}
template<class F> static void rejects(F fn){bool threw=false;try{fn();}catch(const std::exception &){threw=true;}require(threw,"Invalid input was silently accepted");}
static const std::string data="# Hz S RI R 50\n1 .1 0\n2 .2 0\n";
int main(int argc,char **argv){
  if(argc!=3)return 2;dir=pathFromUtf8(argv[2]);fs::create_directories(dir);
  std::map<std::string,std::function<void()>> tests{
    {"snapshot",[]{auto p=write("same.s1p",data);auto a=Cache::open(p,dir/"cache",nullptr,true);auto before=a->trace({{0,1}},{{0,1}});write("same.s1p","# Hz S RI R 50\n1 .7 0\n2 .8 0\n");auto b=Cache::open(p,dir/"cache",nullptr,true);require(b->trace({{0,1}},{{0,1}})!=before,"Fixture unchanged");require(a->trace({{0,1}},{{0,1}})==before,"Open Revision changed when another cache generation replaced its path");}},
    {"cr_only",[]{auto p=write("cr.s1p","! Comment\r# Hz S RI R 50\r1 .1 0\r2 .2 0\r");auto c=Cache::open(p,dir/"cache");require(c->meta().points==2 && c->frequencies()==std::vector<double>({1,2}),"CR-only file changed");}},
    {"false_noise",[]{std::string s="# Hz S RI R 50\n10 .1 0 .2 0 .3 0 .4 0\n";for(int i=1;i<=5;++i)s+=std::to_string(i)+" .1 0 .2 0 .3 0 .4 0\n";rejects([&]{Cache::open(write("decreasing.s2p",s),dir/"cache");});}},
    {"valid_noise",[]{auto p=write("noise.s2p","# GHz S RI R 50\n2 .1 0 .2 0 .3 0 .4 0\n22 .1 0 .2 0 .3 0 .4 0\n! Noise follows\n4 .7 .64 69 .38\n18 2.7 .46 -33 .40\n");require(Cache::open(p,dir/"cache")->meta().points==2,"Valid legacy noise rejected");}},
    {"empty_tdr",[]{Settings s;rejects([&]{analyze(Trace{},Metric::TDR,s);});}},
    {"truncated_live_cache",[]{auto c=Cache::open(write("trunc.s1p",data),dir/"cache");std::error_code ec;fs::resize_file(c->path(),512,ec);
#ifdef _WIN32
      if(ec){std::cout<<"OS protected open cache from truncation\n";return;}
#endif
      require(!ec,"Could not create truncation fixture");rejects([&]{c->trace({{0,1}},{{0,1}});});}},
    {"invalid_cached_frequency",[]{auto c=Cache::open(write("freq.s1p",data),dir/"cache");std::fstream f(c->path(),std::ios::in|std::ios::out|std::ios::binary);
#ifdef _WIN32
      if(!f){std::cout<<"OS protected open cache from modification\n";return;}
#endif
      require(bool(f),"Could not create corrupt frequency fixture");double invalid=NaN;f.seekp(512);f.write(reinterpret_cast<char*>(&invalid),8);f.close();rejects([&]{c->frequencies();});}},
    {"zero_transfer",[]{Trace t;t.x={0,1,2};t.s={.1,0,.1};Settings s;s.il={true,-3,{}};auto r=analyze(t,Metric::IL,s);require(r.status=="NG"&&r.worst==-std::numeric_limits<double>::infinity()&&r.worstX==1,"Zero IL not treated as failing notch");}},
    {"cancel_reopen",[]{auto p=write("cancel.s1p",data);Cache::open(p,dir/"cache");Control ctl;ctl.cancelled=true;rejects([&]{Cache::open(p,dir/"cache",&ctl);});}},
    {"parallel_import",[]{
      auto p=write("parallel.s1p",data);
      std::vector<std::shared_ptr<Cache>> v(16);
      std::vector<std::exception_ptr> errors(16);
      std::vector<std::jthread> threads;
      for(int i=0;i<16;++i)
        threads.emplace_back([&,i]{
          try{v[i]=Cache::open(p,dir/"cache");}
          catch(...){errors[i]=std::current_exception();}
        });
      threads.clear();
      int created=0;
      for(int i=0;i<16;++i){
        if(errors[i])std::rethrow_exception(errors[i]);
        require(v[i]->trace({{0,1}},{{0,1}})[0]==Complex(.1,0),"Concurrent import changed data");
        if(!v[i]->reused())++created;
      }
      require(created==1,"Concurrent import created the same cache more than once");
      std::cout<<"Cache generations created="<<created<<", reused="<<(16-created)<<'\n';
    }}
  };
  try{tests.at(argv[1])();std::cout<<"PASS "<<argv[1]<<'\n';return 0;}catch(const std::exception &e){std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<'\n';return 1;}
}
