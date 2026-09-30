#pragma once
#include "app.hpp"
inline void checkSettingsParsing() {
  auto rejects = [](auto fn) { bool rejected=false;try{fn();}catch(const std::exception &){rejected=true;}if(!rejected)throw si::Error("Malformed project setting was silently coerced"); };
  rejects([]{settingsFromJson(QJsonObject{{"limits",QJsonArray{QJsonObject{{"enabled",true},{"constant","invalid"}}}}});});
  for(auto key:{"startHz","markers","targetOhm","tdrStart","kaiserBeta"})
    rejects([&]{settingsFromJson(QJsonObject{{key,"invalid"}});});
  rejects([]{settingsFromJson(QJsonObject{{"startHz",-1}});});
  rejects([]{settingsFromJson(QJsonObject{{"tdrStart",2e-9},{"tdrStop",1e-9}});});
  rejects([]{settingsFromJson(QJsonObject{{"reverse","false"}});});
  rejects([]{settingsFromJson(QJsonObject{{"quick",QJsonArray{"TDR"}}});});
  rejects([]{settingsFromJson(QJsonObject{{"manualMarkersHz",QJsonArray{"bad"}}});});
  rejects([]{channelFromJson(QJsonObject{{"nearP",0},{"farP",1.5}});});
  si::Settings original;original.tdrStopSeconds=3e-9;original.tdrTerminations={si::Termination::Resistor,si::Termination::Split};
  auto restored=settingsFromJson(settingsJson(original));
  if(settingsJson(restored)!=settingsJson(original))throw si::Error("Valid project settings changed on roundtrip");
  auto legacy=settingsFromJson(QJsonObject{});
  if(legacy.tdrTerminations!=std::vector<si::Termination>{si::Termination::Reference})throw si::Error("Legacy termination default changed");
}
