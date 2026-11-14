#include "si/core.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
using namespace si;
int main(int argc,char**argv){
 try {
  std::vector<std::string> values={"plain","=1+2","+SUM(1,2)","-1+2","@SUM(1,2)"," =1+2","\t=1+2","\r\n=1+2","＝1+2","'","'=1+2","''=1+2","John's Channel","quoted \"name\"","comma,=1+2","line\r\nbreak","한글 채널",""," leading and trailing ","00123","12345678901234567890","1E10","01/02","TRUE","line\nbreak","line\rbreak","＋1+2","－1+2","＠SUM(1,2)",std::string(4096,'L'),"%0D%0A","100%"};
  std::vector<Channel> original;
  for(size_t i=0;i<values.size();++i){Channel c;c.name=values[i]; c.group=c.alias=c.id=values[i];if(c.name.empty()){c.name="Empty metadata";c.id="empty:metadata";}c.nearP=4*i;c.nearN=4*i+1;c.farP=4*i+2;c.farN=4*i+3;original.push_back(c);}
  if(argc==3 && std::string(argv[1])=="generate"){writeMappingCsv(argv[2],original);std::cout<<"GENERATED "<<original.size()<<" rows with production v"<<version<<" writeMappingCsv\n";return 0;}
  if(argc!=2)return 2;
  auto got=readMappingCsv(argv[1]);int failures=0;
  if(got.size()!=original.size()){std::cout<<"ROW_COUNT expected="<<original.size()<<" actual="<<got.size()<<"\n";return 1;}
  for(size_t i=0;i<got.size();++i){auto&a=original[i];auto&b=got[i];
   for(auto [name,p]:std::vector<std::pair<const char*,std::string Channel::*>>{{"Channel",&Channel::name},{"Group",&Channel::group},{"Alias",&Channel::alias},{"ID",&Channel::id}})if(a.*p!=b.*p){++failures; std::cout<<"MISMATCH row="<<i+2<<" field="<<name<<" expected="<<std::quoted(a.*p)<<" actual="<<std::quoted(b.*p)<<"\n";}
   if(a.nearP!=b.nearP||a.nearN!=b.nearN||a.farP!=b.farP||a.farN!=b.farN){++failures;std::cout<<"PORT_MISMATCH row="<<i+2<<"\n";}
  }
  std::cout<<"ROWS="<<got.size()<<" TEXT_FIELDS="<<got.size()*4<<" PORT_FIELDS="<<got.size()*4<<" MISMATCHES="<<failures<<"\n";
  Metadata m;m.ports=4*got.size();m.reference.resize(m.ports,50);validateMapping(got,m);
  return failures?1:0;
 }catch(const std::exception&e){std::cerr<<"ERROR "<<e.what()<<"\n";return 1;}
}
