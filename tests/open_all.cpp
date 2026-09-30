#include "app.hpp"
#include <iostream>
struct NavigationProbe { static bool busy(MainWindow&w){return w.busy;} static auto& revisions(MainWindow&w){return w.revisions;} static void clean(MainWindow&w){w.dirty=false;} };
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("SParamView-AllFiles-Verification");QStandardPaths::setTestModeEnabled(true);applyLightTheme(app);
 QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");QFont::insertSubstitution("Segoe UI","NanumGothic");
 if(argc<3)return 2;QString output=QString::fromLocal8Bit(argv[1]);QDir().mkpath(output);QStringList paths;for(int i=2;i<argc;++i)paths<<QString::fromLocal8Bit(argv[i]);
 MainWindow w;w.show();QTimer timer;QElapsedTimer clock;clock.start();bool seen=false,warning=false,done=false;int rows=-1;QStringList errors;double maxGap=0,last=0;
 QObject::connect(&timer,&QTimer::timeout,[&]{
  if(done)return;double now=clock.elapsed();maxGap=std::max(maxGap,now-last);last=now;
  for(auto *widget:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(widget);box&&box->isVisible()){errors<<box->text();box->accept();}
  if(auto *dialog=w.findChild<QDialog*>("channelMappingDialog");dialog&&dialog->isVisible()&&!seen){rows=dialog->findChild<QTableWidget*>()->rowCount();warning=dialog->findChild<QLabel*>("mappingWarning")!=nullptr;dialog->grab().save(output+"/Mapping.png");seen=true;dialog->reject();}
  if((!NavigationProbe::busy(w)&&clock.elapsed()>1000)||clock.elapsed()>180000){
   done=true;timer.stop();QJsonArray revisions;
   for(auto&r:NavigationProbe::revisions(w))revisions.append(QJsonObject{{"name",r.name},{"ports",int(r.cache->meta().ports)},{"points",double(r.cache->meta().points)},{"channels",int(r.channels.size())},{"confirmed",r.confirmed},{"mapping_warning",r.mappingWarning}});
   bool ok=errors.empty()&&revisions.size()==paths.size()&&!NavigationProbe::busy(w);
   QJsonObject log{{"application",si::version},{"passed",ok},{"input_count",paths.size()},{"revisions",revisions},{"mapping_dialog_seen",seen},{"initial_mapping_rows",rows},{"warning_seen",warning},{"errors",QJsonArray::fromStringList(errors)},{"elapsed_ms",double(clock.elapsed())},{"heartbeat_max_gap_ms",maxGap}};
   QFile file(output+"/Open.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(log).toJson());file.close();w.grab().save(output+"/Application.png");std::cout<<QJsonDocument(log).toJson().constData();NavigationProbe::clean(w);app.exit(ok?0:1);
  }
 });timer.start(20);QTimer::singleShot(0,[&]{w.openPaths(paths);});return app.exec();
}
