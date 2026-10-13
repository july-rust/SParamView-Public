#include "app.hpp"
#include <fstream>
#include <iostream>
struct NavigationProbe{
 static bool busy(MainWindow&w){return w.busy;}
 static std::vector<Revision>& revisions(MainWindow&w){return w.revisions;}
 static void clean(MainWindow&w){w.dirty=false;}
};
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("SParamView-OpenTests");
 QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");
 QFont::insertSubstitution("Segoe UI","NanumGothic");QStandardPaths::setTestModeEnabled(true);applyLightTheme(app);
 if(argc<3)return 2;QString folder=QString::fromLocal8Bit(argv[1]);QDir().mkpath(folder);
 QString mode=QString::fromLocal8Bit(argv[2]);QStringList paths;
 for(int i=3;i<argc;++i)paths<<QString::fromLocal8Bit(argv[i]);
 if(mode=="fallback"||mode=="batch"){
  auto file=fp(folder+"/ambiguous.s4p");std::ofstream out(file);
  out<<"! Port 1 = BGA-1 CLK_POS\n! Port 2 = BGA-2 CLK_NEG\n! Port 3 = U1-1 CLK_POS\n! Port 4 = U1-2 CLK_NEG\n!.DiffChannels\n"
  "!\"D\" \"CLK_POS\" \"CLK_NEG\" \"a\" \"b\" \"c\" \"d\" \"0\" \"1\" \"2\" \"3\"\n!.EndDiffChannels\n# Hz S RI R 50\n";
  for(int i=0;i<32;++i){out<<i*1e8;for(int j=0;j<16;++j)out<<" 0 0";out<<'\n';}out.close();paths<<qs(si::utf8(file));
 }
 if(mode=="signaltype"){
  auto file=fp(folder+"/regular-pn.s4p");std::ofstream out(file);
  out<<"! Port 1 = U1-A DATAP0\n! Port 2 = U1-B DATAN0\n! Port 3 = J1-1 DATAP0\n! Port 4 = J1-2 DATAN0\n# Hz S RI R 50\n";
  for(int i=0;i<32;++i){out<<i*1e8;for(int j=0;j<16;++j)out<<" 0 0";out<<'\n';}out.close();paths<<qs(si::utf8(file));
 }
 MainWindow w;w.show();QElapsedTimer clock;clock.start();QTimer timer;
 bool mappingSeen=false,signalSeen=false,warningSeen=false,labelErrorSeen=false,saved=false,done=false,clicked=false;QString failure;int rows=-1;
 QObject::connect(&timer,&QTimer::timeout,[&]{
  if(done)return;
  for(auto *widget:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(widget);box&&box->isVisible()){
   // A separate message box may also appear; never erase a prior match.
   labelErrorSeen = labelErrorSeen ||
       box->text().contains("Cannot establish a unique P/N polarity") ||
       box->text().contains("Cannot establish P/N polarity");
   if(!clicked)failure="Unexpected open error: "+box->text();box->accept();
  }
  auto *signalDialog=w.findChild<QDialog*>("signalTypeDetectionDialog");
  if(mode=="signaltype"&&signalDialog&&signalDialog->isVisible()&&!signalSeen){
   auto *selector=signalDialog->findChild<QComboBox*>("signalTypeSelector");
   if(!selector||selector->currentData().toString()!="Differential")failure="Signal Type Detection did not preselect Differential";
   signalSeen=true;
   if(auto *buttons=signalDialog->findChild<QDialogButtonBox*>())
    buttons->button(QDialogButtonBox::Ok)->click();
   else failure="Signal Type Detection buttons missing";
   return;
  }
  auto *dialog=w.findChild<QDialog*>("channelMappingDialog");
  if(dialog&&dialog->isVisible()&&!mappingSeen){
   auto *grid=dialog->findChild<QTableWidget*>();rows=grid->rowCount();warningSeen=dialog->findChild<QLabel*>("mappingWarning")!=nullptr;
   bool fallback=mode=="fallback"||mode=="batch";
   if(fallback&&!clicked){
    if(rows!=0||!warningSeen)failure="Explicit differential metadata incorrectly accepted a single-ended compatibility fallback";
    grid->setRowCount(0); // Auto candidates are intentionally empty; test manual-row preservation.
    for(auto *b:dialog->findChildren<QPushButton*>())if(b->text()=="Add Row")b->click();
    if(grid->rowCount()!=1)failure="Add row failed";
    QStringList v{"MANUAL","1","2","3","4","Default","","manual"};for(int j=0;j<v.size();++j)grid->setItem(0,j,new QTableWidgetItem(v[j]));
    clicked=true;for(auto *b:dialog->findChildren<QPushButton*>())if(b->text()=="Suggest from Labels"){QTimer::singleShot(0,b,[b]{b->click();});break;}return;
   }
   // The suggestion's error dialog is queued for a later event-loop turn.
   // Do not check rows or submit Save until the expected modal warning has
   // actually been observed and accepted; the old 20 ms timer raced it.
   if(fallback && clicked && !labelErrorSeen && clock.elapsed() < 30000)return;
   dialog->grab().save(folder+"/mapping.png");
   if(fallback){
    const QStringList expected{"MANUAL","1","2","3","4","Default","","manual"};
    bool preserved=grid->rowCount()==1;
    if(preserved)for(int col=0;col<expected.size();++col){
      const auto *item=grid->item(0,col);
      if(!item||item->text()!=expected[col]){preserved=false;break;}
    }
    if(!labelErrorSeen)failure="Expected polarity warning never appeared";
    else if(!preserved)failure="Manual rows lost after suggestion warning";
    else {dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();saved=true;}
   }else if(mode=="signaltype"){
    if(!signalSeen||rows!=1)failure="Signal Type Detection did not lead to one differential candidate";
    if(rows==1){
     QStringList expected{"1","2","3","4"};
     for(int col=1;col<=4;++col)
      if(!grid->item(0,col)||grid->item(0,col)->text()!=expected[col-1])
       failure="Signal Type Detection produced incorrect P/N or Near/Far mapping";
    }
    dialog->reject();
   }else{
    for(const auto&r:NavigationProbe::revisions(w)){
     if(r.cache->meta().ports==16){if(r.channels.size()!=4||r.channels[0].nearP!=4||r.channels[0].nearN!=0||r.channels[0].farP!=12||r.channels[0].farN!=8)failure="CLK mapping";}
     else if(r.cache->meta().ports==28){if(r.channels.size()!=13||r.channels[0].nearP!=13||r.channels[0].nearN!=12||r.channels[0].farP!=27||r.channels[0].farN!=26)failure="INPUT mapping";}
     else failure="Unexpected ports";
    }dialog->reject();
   }mappingSeen=true;
  }
  if((mappingSeen&&!NavigationProbe::busy(w))||clock.elapsed()>30000||!failure.isEmpty()){
   done=true;timer.stop();auto &revs=NavigationProbe::revisions(w);bool ok=failure.isEmpty()&&mappingSeen&&revs.size()==size_t(paths.size());
   if(saved)ok=ok&&revs.back().confirmed&&revs.back().channels.size()==1&&revs.back().mappingWarning.isEmpty();
   if(mode=="signaltype")ok=ok&&signalSeen&&revs.back().signalTypeReviewed&&revs.back().signalType=="Differential";
   QJsonObject r{{"passed",ok},{"mode",mode},{"revisions",int(revs.size())},{"initial_mapping_rows",rows},{"signal_dialog_seen",signalSeen},{"mapping_dialog_seen",mappingSeen},{"warning_seen",warningSeen},{"label_error_caught",labelErrorSeen},{"manual_mapping_saved",saved},{"failure",failure},{"elapsed_ms",double(clock.elapsed())}};
   QFile f(folder+"/open.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(r).toJson());w.grab().save(folder+"/application.png");
   std::cout<<QJsonDocument(r).toJson().constData();NavigationProbe::clean(w);app.exit(ok?0:1);
  }
 });timer.start(20);QTimer::singleShot(0,[&]{w.openPaths(paths);});return app.exec();
}
