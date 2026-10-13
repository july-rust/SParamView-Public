#include "si/core.hpp"
#include "temp_directory.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace si;
static void require(bool ok, const std::string &why) { if (!ok) throw Error(why); }
int main(int argc, char **argv) {
  const std::string test = argc > 1 ? argv[1] : "named";
  const auto dir = fs::temp_directory_path() / ("sparamview-v124-" + test);
  fs::remove_all(dir); fs::create_directories(dir); TestDirectoryCleanup cleanup{dir};
  auto write = [&](const std::string &name, const std::string &text) {
    const auto path = dir / name; std::ofstream(path, std::ios::binary) << text; return path;
  };
  try {
    if (test == "named") {
      Metadata m; m.ports=4; m.reference={50,50,50,50};
      for (const auto &labels : std::vector<std::vector<std::string>>{
          {"DATA_near_P","DATA_near_N","DATA_far_P","DATA_far_N"},
          {"DATA_P_near","DATA_N_near","DATA_P_far","DATA_N_far"},
          {"U1-A DATAP0","U1-B DATAN0","J1-1 DATAP0","J1-2 DATAN0"}}) {
        m.labels=labels; std::string diagnostic;
        auto channels=suggestMapping(m,&diagnostic,false);
        require(!channels.empty(),"Single-ended candidates lost");
        require(std::none_of(channels.begin(),channels.end(),[](const auto &c){return c.differential();}),
                "Single-ended override ignored by directional label path");
      }
    } else if (test == "options") {
      auto p=write("options.s1p","# Hz S RI R 50\n# GHz S DB R 75\n1 .25 0\n# MHz S MA R 100\n2 .5 0\n");
      auto c=Cache::open(p,dir/"cache");
      require(c->frequencies()==std::vector<double>({1,2}),"Later option changed units");
      require(c->meta().reference[0]==50 && c->meta().format=="RI","Later option changed interpretation");
      require(c->meta().warnings.size()>=1,"Ignored option missing diagnostic");
      require(std::abs(c->trace({{0,1}},{{0,1}})[0]-Complex(.25,0))<1e-14,"Later option changed values");
    } else if (test == "triangle") {
      std::vector<std::shared_ptr<Cache>> caches;
      for (const std::string format : {"Full","Lower","Upper"}) {
        std::ostringstream s;
        s << "[Version] 2.1\n# Hz S RI R 50\n[Number of Ports] 4\n[Number of Frequencies] 1\n[Mixed-Mode Order] D1,2 C1,2 S3 S4\n[Matrix Format] " << format << "\n[Network Data]\n1";
        for(int i=0;i<4;++i) for(int j=0;j<4;++j) {
          if((format=="Lower"&&j>i)||(format=="Upper"&&j<i))continue;
          s << ' ' << (1+std::min(i,j)*4+std::max(i,j))*.01 << " .005";
        }
        s << "\n[End]\n"; caches.push_back(Cache::open(write(format+".ts",s.str()),dir/"cache"));
      }
      for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(size_t k=1;k<caches.size();++k)
        require(std::abs(caches[0]->trace({{i,1}},{{j,1}})[0]-caches[k]->trace({{i,1}},{{j,1}})[0])<1e-14,"Mixed-mode triangular mismatch");
    } else if (test == "hash") {
      std::string data="# Hz S RI R 50\n";
      for(int i=1;i<=40000;++i)data+=std::to_string(i)+" .1 0\n";
      auto p=write("large.s1p",data); auto a=Cache::open(p,dir/"cache"); auto time=fs::last_write_time(p);
      auto pos=data.find(".1",80000); require(pos!=std::string::npos,"fixture missing"); data[pos+1]='2';
      write("large.s1p",data); fs::last_write_time(p,time);
      auto b=Cache::open(p,dir/"cache",nullptr,true);
      require(a->meta().sha256!=b->meta().sha256 && b->meta().sha256==sha256File(p),"Full verification missed same-size same-time edit");
    } else if (test == "numbers") {
      for(const std::string value : {"0","+0","-0",".5","1.","+1.25D-2","-3.4e+1"})
        Cache::open(write("valid.s1p","# Hz S RI R 50\n1 "+value+" 0\n"),dir/"cache",nullptr,true);
      for(const std::string value : {"nan","inf","0x1p0","1e9999","1e-9999","++1","+-1","-+1","--1","1x","1e"}) {
        bool rejected=false;try{Cache::open(write("bad.s1p","# Hz S RI R 50\n1 "+value+" 0\n"),dir/"cache",nullptr,true);}catch(const Error&){rejected=true;}
        require(rejected,"Invalid number accepted: "+value);
      }
    } else throw Error("Unknown test case");
    std::cout << "PASS " << test << '\n'; return 0;
  } catch(const std::exception &e) { std::cerr << "FAIL " << test << ": " << e.what() << '\n'; return 1; }
}
