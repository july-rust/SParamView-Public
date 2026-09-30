#include "app.hpp"
#include <iostream>
static void check(bool ok, const char *message) { if (!ok) throw si::Error(message); }
struct WorkspaceProbe {
 static void settle(MainWindow &w) {
  QElapsedTimer t; t.start();
  do { QApplication::processEvents(QEventLoop::AllEvents, 10); QThread::msleep(1);
   check(t.elapsed() < 120000, "Workspace operation timed out");
  } while (w.busy || w.viewRunning || t.elapsed() < 200);
 }
 static QByteArray values(MainWindow &w) {
  QJsonArray a;
  for (auto&r:w.results) a.append(QJsonObject{{"channel",qs(r.channel)},{"revision",qs(r.revision)},
   {"metric",int(r.metric)},{"worst",r.worst},{"worst_x",r.worstX},{"status",qs(r.status)},
   {"margin",r.margin},{"parameter",qs(r.parameter)},{"termination",int(r.termination)}});
  return QJsonDocument(a).toJson(QJsonDocument::Compact);
 }
 static QJsonObject run(MainWindow &w, const QString &project, const QString &out, QSize size) {
  w.resetWorkspaceLayout(); w.resize(size); w.show(); settle(w);
  w.loadProject(project); settle(w); check(!w.revisions.empty(),"Project missing");
  // Regression: RL -> IL -> NEXT -> FEXT -> TDR must calculate each missing
  // metric and immediately form a graph, even when the previous result exists.
  w.results.clear(); w.lastJobs.clear();
  for(int i=0;i<5;++i){
   w.metricButtons[i]->click(); settle(w);
   check(!w.results.empty(),"Individual metric analysis returned no results");
   check(std::all_of(w.results.begin(),w.results.end(),[i](const si::Result&r){return int(r.metric)==i;}),"Individual metric button did not calculate requested metric");
   check(!w.plot->snapshot.curves.empty(),"Individual metric analysis did not form graph");
  }
  // And make sure returning to RL is a fresh valid calculation/display path.
  w.metricButtons[0]->click(); settle(w);
  check(w.displayed==si::Metric::RL&&!w.plot->snapshot.curves.empty(),"RL return after sequential metric run failed");
  w.run(w.settings.quick); settle(w); check(!w.results.empty(),"Analysis missing");
  if (w.revisions.size() > 1) {
   const int originalRevision=w.revisionBox->currentIndex();
   const int otherRevision=originalRevision==0?1:0;
   const auto resultCount=w.results.size();
   w.revisionBox->setCurrentIndex(otherRevision);settle(w);
   const auto active=ss(w.revisions[size_t(otherRevision)].name);
   check(!w.resultRows.empty(),"Revision switch produced no visible completed results");
   check(std::all_of(w.resultRows.begin(),w.resultRows.end(),[&](size_t row){return w.results[row].revision==active;}),
         "Result table leaked a previous revision after Quick Analysis");
   w.metricButtons[0]->click();settle(w);
   check(w.displayed==si::Metric::RL&&!w.plot->snapshot.curves.empty(),
         "Individual RL did not display after revision switch");
   check(std::all_of(w.plot->snapshot.curves.begin(),w.plot->snapshot.curves.end(),[&](const si::Result&r){return r.revision==active;}),
         "Plot leaked a previous revision after individual metric selection");
   check(w.results.size()==resultCount,
         "Current-revision completed RL was incorrectly replaced by a foreign/global availability decision");
   w.revisionBox->setCurrentIndex(originalRevision);settle(w);
   const auto restored=ss(w.revisions[size_t(originalRevision)].name);
   check(std::all_of(w.resultRows.begin(),w.resultRows.end(),[&](size_t row){return w.results[row].revision==restored;}),
         "Returning to the original revision did not restore its result view");
  }
  w.table->selectRow(0); settle(w);
  auto original=values(w); auto jobs=w.lastJobs.size(); auto settings=settingsJson(w.lastSettings);
  auto selection=w.table->selectionModel()->selectedRows(); check(selection.size()==1,"Selection missing");
  w.plot->setView(1e9,4e9,-30,1); settle(w);
  auto split=w.analysisSplit->sizes(); auto horizontal=w.horizontalSplit->sizes();
  QSize normalPlot=w.plot->size(), normalTable=w.table->size();
  auto area=graphArea(w.plot->rect(),w.plot->snapshot);
  QJsonObject result{{"requested_width",size.width()},{"requested_height",size.height()},
   {"actual_width",w.width()},{"actual_height",w.height()},
   {"normal_plot_width",normalPlot.width()},{"normal_plot_height",normalPlot.height()},
   {"normal_graph_area",area.width()*area.height()},{"normal_table_height",normalTable.height()},
   {"analysis_rows",w.table->rowCount()}};
  check(w.width()<=size.width() && w.height()<=size.height(),"Layout exceeds requested screen size");
  w.grab().save(out+"/Split.png");
  QStringList expectedQuick{"RL","IL","NEXT","FEXT","TDR"};
  for(int i=0;i<expectedQuick.size();++i){
   auto quick=w.findChild<QCheckBox*>(QString("quickMetric%1").arg(i));
   check(quick&&quick->text()==expectedQuick[i],"Compact Quick Analysis control labels");
  }
  auto compactSettings=w.findChild<QScrollArea*>("analysisSettingsPanel");
  check(compactSettings&&compactSettings->maximumHeight()<=190,"Compact analysis settings preserve more plot height");
  result["compact_quick_analysis_ui"]=true;

  auto advancedButton=w.findChild<QToolButton*>("advancedToggle");
  auto advancedPanel=w.findChild<QWidget*>("advancedSettings");
  QToolButton *resultsButton=nullptr;
  for(auto tool:w.findChildren<QToolButton*>())if(tool->defaultAction()==w.workspaceActions[2])resultsButton=tool;
  check(advancedButton&&advancedPanel&&resultsButton,"Advanced controls missing");
  auto resultsPos=resultsButton->mapTo(&w,QPoint(0,0)), advancedPos=advancedButton->mapTo(&w,QPoint(0,0));
  check(std::abs((advancedPos.y()+advancedButton->height()/2)-(resultsPos.y()+resultsButton->height()/2))<=2 && advancedPos.x()>resultsPos.x()+resultsButton->width() && advancedPos.x()-resultsPos.x()-resultsButton->width()<=12,"Advanced is not adjacent to Maximize Results");
  advancedButton->click();settle(w);
  check(advancedPanel->isVisible()&&w.settingsPanel->isVisible(),"Advanced did not reveal settings");
  check(w.width()<=size.width()&&w.height()<=size.height(),"Advanced exceeds viewport");
  w.grab().save(out+"/Advanced.png");
  w.settingsAction->setChecked(false);settle(w);
  check(!advancedButton->isChecked()&&!advancedPanel->isVisible(),"Advanced state did not reset");
  check(values(w)==original&&settingsJson(w.lastSettings)==settings,"Advanced altered analysis");
  result["advanced_adjacent_and_functional"]=true;
  // v1.1.1 keeps the compact settings strip visible in normal Split view.
  // Restore that baseline before entering temporary Plot/Results focus modes.
  w.settingsAction->setChecked(true); settle(w);
  split=w.analysisSplit->sizes();

  w.workspaceActions[1]->trigger(); settle(w);
  check(w.plot->isVisible()&&!w.resultsPanel->isVisible()&&!w.channelPanel->isVisible(),"Graph focus visibility");
  check(w.plot->height()>normalPlot.height()&&w.plot->width()>normalPlot.width(),"Graph did not grow");
  result["focused_plot_width"]=w.plot->width();result["focused_plot_height"]=w.plot->height();
  auto large=graphArea(w.plot->rect(),w.plot->snapshot);result["focused_graph_area"]=large.width()*large.height();
  check(w.plot->snapshot.viewStart==1e9&&w.plot->snapshot.viewStop==4e9&&w.plot->snapshot.viewBottom==-30,"Focus reset axes");
  w.grab().save(out+"/Graph.png");
  w.workspaceActions[2]->trigger(); settle(w);
  check(!w.tabs->isVisible()&&w.table->isVisible(),"Table focus visibility");
  advancedButton->click();settle(w);
  check(advancedPanel->isVisible()&&w.settingsPanel->isVisible(),"Advanced inaccessible from results focus");
  w.settingsAction->setChecked(false);settle(w);

  check(w.table->height()>normalTable.height()&&w.table->width()>normalTable.width(),"Table did not grow");
  result["focused_table_width"]=w.table->width();result["focused_table_height"]=w.table->height();
  w.tableTextSize->setCurrentIndex(w.tableTextSize->findData(18));settle(w);
  check(w.table->verticalHeader()->defaultSectionSize()==34,"Table font row size");
  check(w.table->selectionModel()->selectedRows()==selection,"Font/focus changed selection");
  w.grab().save(out+"/Results.png");
  w.workspaceActions[0]->trigger();settle(w);
  auto restored=w.analysisSplit->sizes();
  check(restored.size()==2&&std::abs(restored[0]-split[0])<=2,"Split proportions not restored");
  check(w.channelPanel->isVisible()&&w.settingsPanel->isVisible(),"Panels not restored");
  check(w.plot->snapshot.viewStart==1e9&&w.plot->snapshot.viewStop==4e9,"Table focus reset axes");
  for(int i=0;i<15;++i){w.setWorkspaceMode(1);w.setWorkspaceMode(2);w.setWorkspaceMode(0);}
  settle(w);
  check(values(w)==original&&w.lastJobs.size()==jobs&&settingsJson(w.lastSettings)==settings,"Layout altered analysis or limits");
  check(w.table->selectionModel()->selectedRows()==selection,"Repeated switching lost selection");
  auto before=w.analysisSplit->sizes();
  w.analysisSplit->setSizes({before[0]+60,std::max(100,before[1]-60)});settle(w);
  check(w.analysisSplit->sizes()!=before,"Vertical splitter not adjustable");
  w.settingsAction->setChecked(true);settle(w);
  check(w.quickButton->mapTo(w.channelPanel,QPoint(0,0)).y()>=0 && w.quickButton->mapTo(w.channelPanel,QPoint(0,w.quickButton->height())).y()<=w.channelPanel->height(),"Quick Analysis button exceeds channel viewport");
  check(w.settingsPanel->isVisible()&&w.height()<=size.height(),"Expanded settings exceed viewport");
  auto scroll=qobject_cast<QScrollArea*>(w.settingsPanel);check(scroll&&scroll->widget(),"Settings scroll container missing");
  w.grab().save(out+"/Settings.png");w.settingsAction->setChecked(false);settle(w);
  check(!w.analysisSplit->opaqueResize(),"Splitter triggers continuous redraw");
  auto normalGeometry=w.geometry();
  const bool offscreen=QGuiApplication::platformName().compare("offscreen",Qt::CaseInsensitive)==0;
  if(!offscreen){
   w.toggleFullscreen();settle(w);check(w.isFullScreen(),"Fullscreen enter");
   w.toggleFullscreen();settle(w);check(!w.isFullScreen(),"Fullscreen exit");
   check(w.size()==normalGeometry.size(),"Fullscreen changed normal size");
  }else{
   result["fullscreen_headless_skipped"]=true;
  }
  // Separate metric selection reads completed data without starting new jobs.
  auto selectedMetric=si::Metric::TDR;w.metricView->setCurrentIndex(int(selectedMetric));settle(w);
  check(w.displayed==selectedMetric&&values(w)==original&&w.lastJobs.size()==jobs,"Metric selector changed analysis");
  w.table->clearSelection(); w.table->selectRow(0); settle(w);
  check(w.metricView->currentIndex()==int(w.displayed),"Table selection left stale metric label");
  if (w.revisions.size() > 1) {
   w.showMetric(si::Metric::RL);settle(w);w.setWorkspaceMode(2);
   w.compareRevisions();settle(w);
   check(w.tabs->isVisible()&&w.tabs->currentIndex()==1,"Comparison hidden by table focus");
  }
  w.setWorkspaceMode(2);w.saveWorkspaceLayout();
  check(w.workspaceMode==0,"Saving focus did not normalize layout");
  result["axes_selection_results_preserved"]=true;result["divider_adjustable"]=true;result["fullscreen_restored"]=true;
  result["metric_switch_without_analysis"]=true;result["passed"]=true;w.dirty=false;return result;
 }
 static void persistence(MainWindow&w) {
  check(w.tableTextSize->currentData().toInt()==18,"Table font preference not restored");
  check(w.workspaceMode==0,"Unexpected startup focus");
  w.dirty=false;
 }
};
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("SParamView-WorkspaceTests");
 if(argc<3)return 2;QString out=QString::fromLocal8Bit(argv[2]);QDir().mkpath(out);
 QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,out+"/preferences");
 {QSettings p(QSettings::IniFormat,QSettings::UserScope,"SParamView",app.applicationName());p.clear();}
 QLocale::setDefault(QLocale(QLocale::German,QLocale::Germany));
 applyLightTheme(app);check(QLocale().decimalPoint()==".","Technical UI must use decimal points");QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");QFont::insertSubstitution("Segoe UI","NanumGothic");
 try{
  QSize size(argc>3?QString::fromLocal8Bit(argv[3]).toInt():1440,argc>4?QString::fromLocal8Bit(argv[4]).toInt():980);
  QJsonObject result;
  {MainWindow w;result=WorkspaceProbe::run(w,QString::fromLocal8Bit(argv[1]),out,size);}
  {MainWindow w;WorkspaceProbe::persistence(w);}
  result["preferences_restored"]=true;QFile f(out+"/Workspace.json");check(f.open(QIODevice::WriteOnly),"Cannot save QA");f.write(QJsonDocument(result).toJson());std::cout<<QJsonDocument(result).toJson().constData();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
