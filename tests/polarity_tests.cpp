#include "si/core.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
using namespace si;
static int checks=0;
static void require(bool ok){++checks;if(!ok)throw Error("polarity regression failed");}
static Metadata fixture(std::string a,std::string b){
 Metadata m;m.ports=4;m.reference={50,50,50,50};
 m.labels={"BGA-1 "+a,"BGA-2 "+b,"U1-1 "+a,"U1-2 "+b};std::ostringstream line;
 for(auto s:std::vector<std::string>{"DIFF",a,b,"nearA","nearB","farA","farB","0","1","2","3"})line<<std::quoted(s)<<' ';
 m.differentialHints={line.str()};return m;
}
static void rejected(const Metadata&m){bool threw=false;try{suggestMapping(m);}catch(const Error&){threw=true;}require(threw);}
int main(){try{
 struct Case{const char*a,*b,*name;bool positive;};
 for(auto c:std::vector<Case>{{"DATA_P","DATA_N","DATA",true},{"DATA_N","DATA_P","DATA",false},
 {"CLK_N_OUT_0_MP","CLK_P_OUT_0_MP","CLK_OUT_0_MP",false},{"DPHY_CLK_P_MP_I","DPHY_CLK_N_MP_I","DPHY_CLK_MP_I",true},
 {"P_CLK","N_CLK","CLK",true},{"clk_p_out","clk_n_out","clk_out",true},{"CLK_P_1","CLK_N_1","CLK_1",true}}){
  auto cs=suggestMapping(fixture(c.a,c.b));require(cs.size()==1);auto &ch=cs[0];require(ch.name==c.name);
  require(ch.nearP==(c.positive?0:1));require(ch.nearN==(c.positive?1:0));require(ch.farP==(c.positive?2:3));require(ch.farN==(c.positive?3:2));
 }
 require(suggestMapping(fixture("DATA_P","DATA_N"))[0].id=="net:data");
 for(auto [a,b]:std::vector<std::pair<std::string,std::string>>{{"CLK_POS","CLK_NEG"},{"CLK_P_MP","CLK_N_OTHER"},{"AP","AN"},{"P","N"},{"CLK_P_P","CLK_N_N"},{"CLK_P","CLK_P"},{"CLK_P0","CLK_N0"},{"CLK_P_MP","CLK_N_MP_EXTRA"}})rejected(fixture(a,b));
 auto m=fixture("CLK_P_MP","CLK_N_MP");m.labels[0]="BGA-1 OTHER";rejected(m);
 m=fixture("CLK_P_MP","CLK_N_MP");m.reference[1]=75;rejected(m);
 m=fixture("CLK_P_MP","CLK_N_MP");m.differentialHints.push_back(m.differentialHints[0]);rejected(m);
 m=fixture("CLK_P_MP","CLK_N_MP");m.ports=3;rejected(m);
 m=fixture("CLK_P_MP","CLK_N_MP");m.differentialHints[0]="\"malformed\"";rejected(m);
 m=fixture("DPHY_CLK_P_MP_I","DPHY_CLK_N_MP_I");m.ports=6;m.reference.resize(6,50);m.labels.push_back("BGA-3 A_I<0>");m.labels.push_back("U1-3 A_I<0>");
 auto mixed=suggestMapping(m);require(mixed.size()==2);require(mixed[0].group=="Differential");require(mixed[1].group=="Single-ended");require(mixed[1].nearP==4&&mixed[1].farP==5);
 std::cout<<"PASS "<<checks<<" polarity checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
