#include "app.hpp"
#include "../src/ui_language.hpp"
#include <QTextLayout>
#include <QGlyphRun>
#include <iostream>
void applyCompactAnalysisSettings(MainWindow &window);
static void check(bool ok, const char *message) { if (!ok) throw si::Error(message); }
static void localizationCatalogQualification() {
  using namespace sparamview::ui_language;
  QSet<QString> keys;
  static const QRegularExpression placeholder(QStringLiteral("%[1-9][0-9]?"));
  auto placeholders=[](const QString &source) {
    QStringList values;
    auto matches=placeholder.globalMatch(source);
    while(matches.hasNext()) values.append(matches.next().captured());
    values.sort();
    return values;
  };
  for(const auto &entry:catalogEntries()) {
    const QString en=QString::fromUtf8(entry.en),ko=QString::fromUtf8(entry.ko),ja=QString::fromUtf8(entry.ja);
    check(!en.trimmed().isEmpty() && !ko.trimmed().isEmpty() && !ja.trimmed().isEmpty(),"Empty localization catalog entry");
    check(!keys.contains(en),"Duplicate localization catalog source");
    keys.insert(en);
    check(placeholders(en)==placeholders(ko) && placeholders(en)==placeholders(ja),"Localization placeholder mismatch");
    check(menuText(en,Language::Korean)==ko && menuText(en,Language::Japanese)==ja,"Localization lookup mismatch");
  }
  check(keys.size()>=930,"Localization catalog unexpectedly reduced");
  const QString wrapper="Saved mapping invalidated: source changed or stored ports are no longer valid. ";
  const QString evidence="Current source re-evaluated; previous mapping was discarded. ";
  const QString suffix=" User confirmed the signal type before Channel Mapping.";
  const QString type="Differential";
  const QString descriptor="Touchstone [Mixed-Mode Order] S/D/C descriptors declare %1. P/N membership and Near/Far direction are separate.";
  const QString raw=wrapper+evidence+descriptor.arg(type)+suffix;
  const QString opaque="C:/measurements/%1_日本語_한글.s36p";
  const QString portPrefix="Touchstone not found: ";
  for(const auto lang:{Language::English,Language::Korean,Language::Japanese}) {
    const QString expected=menuText(wrapper,lang)+menuText(evidence,lang)+menuText(descriptor,lang).arg(menuText(type,lang))+menuText(suffix,lang);
    check(diagnosticText(raw,lang)==expected,"Composite mapping diagnostic localization failed");
    check(diagnosticText(wrapper+portPrefix+opaque,lang)==menuText(wrapper,lang)+menuText(portPrefix,lang)+opaque,"Composite diagnostic changed opaque filename");
    check(diagnosticText(opaque,lang)==opaque,"Unknown diagnostic unexpectedly changed");
    const QString tdr="Nonuniform low-pass step; original frequency intervals; even-real DC extrapolation; Kaiser beta=%1; quadrature error=%2; interpolation sensitivity=%3. Preview; PowerSI/VNA correlation pending.";
    const QString singular=" Impedance display singular/out of range; rho retained; invalid ohm samples omitted.";
    check(diagnosticText(tdr.arg("6.000000","0.000001","0.000002")+singular,lang)==menuText(tdr,lang).arg("6.000000","0.000001","0.000002")+menuText(singular,lang),"Nonuniform TDR diagnostic mismatch");
    const QString preview="Preview Kaiser beta=%1; reference-tool release validation pending.";
    const QString dc="DC extrapolation and/or complex grid resampling used. ";
    check(diagnosticText(dc+preview.arg("6.000000"),lang)==menuText(dc,lang)+menuText(preview,lang).arg("6.000000"),"Preview TDR diagnostic mismatch");
    const QString load=" Load: %1; opposite end %2; all other physical ports reference matched.";
    const QString termination="P-N 100 ohm (floating)";
    check(diagnosticText(dc+preview.arg("6.000000")+load.arg(termination,"Far"),lang)==
          menuText(dc,lang)+menuText(preview,lang).arg("6.000000")+menuText(load,lang).arg(menuText(termination,lang),menuText("Far",lang)),"Composite TDR load note mismatch");
    check(diagnosticText(portPrefix+opaque+suffix,lang)==menuText(portPrefix,lang)+opaque+suffix,"Opaque path suffix unexpectedly translated");
    const QString fStart="frequency start %1 Hz requested, source starts at %2 Hz";
    const QString fStop="frequency stop %1 Hz requested, source ends at %2 Hz";
    const QString tStart="TDR start %1 s requested, first evaluated sample is %2 s";
    const QString tStop="TDR stop %1 s requested, computed window ends at %2 s";
    const QString range="Requested range incomplete: %1. Evaluated %2 to %3 %4.";
    const QString ng=" An evaluated limit violation was found, so verdict remains NG.";
    const QString na=" The evaluated portion did not fail, but the overall verdict is N/A.";
    const QStringList rawIssues={fStart.arg("1.000000e+03","1.000000e+06"),fStop.arg("2.000000e+10","1.000000e+10")};
    const QStringList expectedIssues={menuText(fStart,lang).arg("1.000000e+03","1.000000e+06"),menuText(fStop,lang).arg("2.000000e+10","1.000000e+10")};
    const auto rawRange=range.arg(rawIssues.join("; "),"1.000000e+06","1.000000e+10","Hz");
    const auto translatedRange=menuText(range,lang).arg(expectedIssues.join("; "),"1.000000e+06","1.000000e+10","Hz");
    check(diagnosticText(rawRange+ng,lang)==translatedRange+menuText(ng,lang),"Frequency coverage NG diagnostic mismatch");
    check(diagnosticText(rawRange+na,lang)==translatedRange+menuText(na,lang),"Frequency coverage N/A diagnostic mismatch");
    const QStringList tRaw={rawIssues.at(1),tStart.arg("1.000000e-12","2.000000e-12"),tStop.arg("3.000000e-09","2.000000e-09")};
    const QStringList tLocalized={expectedIssues.at(1),menuText(tStart,lang).arg("1.000000e-12","2.000000e-12"),menuText(tStop,lang).arg("3.000000e-09","2.000000e-09")};
    const auto tdrHead=dc+preview.arg("6.000000")+load.arg(termination,"Far");
    const auto translatedHead=menuText(dc,lang)+menuText(preview,lang).arg("6.000000")+menuText(load,lang).arg(menuText(termination,lang),menuText("Far",lang));
    check(diagnosticText(tdrHead+" "+range.arg(tRaw.join("; "),"2.000000e-12","2.000000e-09","s")+na,lang)==
          translatedHead+" "+menuText(range,lang).arg(tLocalized.join("; "),"2.000000e-12","2.000000e-09","s")+menuText(na,lang),"TDR composite coverage diagnostic mismatch");
    const QString limit="Frequency-dependent limit boundary/kink checks at %1 non-source point(s) use complex interpolation; redundant collinear limit points do not alter the evaluation grid.";
    const QString marker=" Manual marker values at non-source frequencies use complex interpolation for display only and do not change pass/fail.";
    check(diagnosticText(limit.arg("12")+" "+rawRange+ng+marker,lang)==menuText(limit,lang).arg("12")+" "+translatedRange+menuText(ng,lang)+menuText(marker,lang),"Combined limit, coverage and marker diagnostic mismatch");
    const auto unknown=range.arg("unknown clause","1","2","Hz");
    check(diagnosticText(unknown,lang)==unknown,"Unknown coverage clause should remain opaque");
  }
}
struct WorkspaceProbe {
 static void stage(const char *name) { std::cerr << "WORKSPACE STAGE: " << name << std::endl; }
 static void widthDiagnostics(MainWindow &w,const char *phase) {
  // Query the layout tree, not only the outer window. A 49-pixel increase in
  // minimumSizeHint after focus changes is the native Cocoa failure signature.
  auto widget = [](const char *name,QWidget *v) {
    if (!v) return;
    std::cerr << " [" << name
              << ":hint=" << v->minimumSizeHint().width()
              << ",min=" << v->minimumWidth()
              << ",size=" << v->width() << "]";
  };
  auto layout = [](const char *name,QLayout *v) {
    if (!v) return;
    std::cerr << " [" << name << ":min=" << v->minimumSize().width()
              << ",hint=" << v->sizeHint().width() << "]";
  };
  auto *left=w.horizontalSplit->widget(0);
  auto *right=w.horizontalSplit->widget(1);
  QLayout *rightLayout=right ? right->layout() : nullptr;
  auto *plotPage=w.tabs->widget(0);
  (void)plotPage;
  std::cerr << "WORKSPACE MINWIDTH PHASE=" << phase;
  widget("window",&w);
  widget("central",w.centralWidget());
  widget("split",w.horizontalSplit);
  widget("left",left);
  widget("right",right);
  widget("tabs",w.tabs);
  widget("plot",w.plot);
  widget("results",w.resultsPanel);
  widget("table",w.table);
  widget("summary",w.summary);
  widget("settings",w.settingsPanel);
  layout("root",w.centralWidget()?w.centralWidget()->layout():nullptr);
  layout("right-layout",rightLayout);
  for (const auto *name : {"workspaceToolbar", "workspaceGraphControls", "workspaceResultControls"}) {
    auto *strip=w.findChild<QScrollArea *>(name);
    layout(name,strip&&strip->widget()?strip->widget()->layout():nullptr);
  }
  std::cerr << std::endl;
 }
 static void assertCompactToolbar(MainWindow &w) {
   // QAction emits changed() on check state transitions. QToolButton may
   // replace manually assigned compact text with QAction::text() unless
   // iconText() is explicitly kept compact for both menu and toolbar usage.
   int located=0;
   for(auto *tool:w.findChildren<QToolButton*>()) {
     auto *action=tool->defaultAction();
     if(!action) continue;
     QString expected;
     if(action==w.workspaceActions[0]) expected="Split";
     else if(action==w.workspaceActions[1]) expected="Plot";
     else if(action==w.workspaceActions[2]) expected="Results";
     else if(action==w.channelsAction) expected="Channels";
     else if(action==w.settingsAction) expected="Settings";
     else if(action==w.fullscreenAction) expected="⛶";
     else continue;
     ++located;
     if(tool->text()!=expected) {
       std::cerr << "WORKSPACE TOOLBAR LABEL DRIFT: expected="
                 << expected.toStdString()
                 << " actual=" << tool->text().toStdString()
                 << " action=" << action->text().toStdString() << std::endl;
     }
     check(tool->text()==expected,"QAction changed compact toolbar button text");
   }
   check(located==6,"Expected all six compact workspace toolbar actions");
 }
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
 static void compactMainWindowQualification(MainWindow &w,const QString &out) {
  stage("012_58 compact main workspace qualification");
  const auto originalSize=w.size();
  const auto originalValues=values(w);
  const auto originalSettings=settingsJson(w.lastSettings);
  const auto originalJobs=w.lastJobs.size();
  // 984x688 is a conservative logical client-area target for 1024x768
  // desktops with window decorations and taskbar. This is not a physical
  // monitor or high-DPI certification.
  const QSize target(984,688);
  for (const auto *code : {"ko","ja","en"}) {
   auto *action=w.findChild<QAction *>(QString("uiLanguage_%1").arg(code));
   check(action!=nullptr,"Missing compact-workspace language action");
   action->trigger(); w.resize(target); settle(w);
   widthDiagnostics(w,code);
   check(w.width()<=target.width() && w.height()<=target.height(),
         "Localized main window exceeds compact work area");
   for (const auto *name : {"workspaceHeader","workspaceToolbar","workspaceGraphControls","workspaceResultControls"}) {
    auto *strip=w.findChild<QScrollArea *>(name);
    check(strip && strip->widget() && strip->isVisible(),"Compact control strip missing");
    check(strip->verticalScrollBar()->maximum()==0,"Compact strip clipped vertically");
    for (auto *control : strip->widget()->findChildren<QWidget *>()) {
     if (!control->isVisibleTo(strip->widget())) continue;
     if (!qobject_cast<QAbstractButton *>(control) &&
         !qobject_cast<QComboBox *>(control) &&
         !qobject_cast<QAbstractSpinBox *>(control)) continue;
     strip->ensureWidgetVisible(control,0,0);
     QApplication::processEvents();
     const QRect bounds(control->mapTo(strip->viewport(),QPoint(0,0)),control->size());
     check(strip->viewport()->rect().adjusted(-1,-1,1,1).contains(bounds),
           "Compact workspace control cannot be reached by scrolling");
    }
    strip->horizontalScrollBar()->setValue(0);
   }
   check(values(w)==originalValues && settingsJson(w.lastSettings)==originalSettings &&
         w.lastJobs.size()==originalJobs,"Compact language switch changed numerical results");
   check(w.grab().save(out+QString("/MainCompact_%1.png").arg(code)),"Cannot save compact workspace screenshot");
  }
  w.resize(originalSize);settle(w);
  assertCompactToolbar(w);
 }
 static QJsonObject run(MainWindow &w, const QString &project, const QString &out, QSize size) {
  stage("show and settle window");
  w.resetWorkspaceLayout(); w.resize(size); w.show(); settle(w);
  stage("load project");
  w.loadProject(project); settle(w); check(!w.revisions.empty(),"Project missing");
  w.resize(size); settle(w);
  // Regression: RL -> IL -> NEXT -> FEXT -> TDR must calculate each missing
  // metric and immediately form a graph, even when the previous result exists.
  w.results.clear(); w.lastJobs.clear();
  stage("individual metrics");
  for(int i=0;i<5;++i){
   w.metricButtons[i]->click(); settle(w);
   check(!w.results.empty(),"Individual metric analysis returned no results");
   check(std::all_of(w.results.begin(),w.results.end(),[i](const si::Result&r){return int(r.metric)==i;}),"Individual metric button did not calculate requested metric");
   check(!w.plot->snapshot.curves.empty(),"Individual metric analysis did not form graph");
  }
  // And make sure returning to RL is a fresh valid calculation/display path.
  w.metricButtons[0]->click(); settle(w);
  check(w.displayed==si::Metric::RL&&!w.plot->snapshot.curves.empty(),"RL return after sequential metric run failed");
  stage("quick analysis");
  widthDiagnostics(w,"before-quick");
  w.run(w.settings.quick); settle(w); check(!w.results.empty(),"Analysis missing");
  widthDiagnostics(w,"after-quick");
  assertCompactToolbar(w);
  compactMainWindowQualification(w,out);
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
  result["platform"]=QGuiApplication::platformName();
  result["font_family"]=w.font().family();
  result["minimum_width"]=w.minimumSizeHint().width();result["minimum_height"]=w.minimumSizeHint().height();
  std::cout<<QJsonDocument(result).toJson().constData()<<std::flush;
  check(w.width()<=size.width() && w.height()<=size.height(),"Layout exceeds requested screen size");
  w.grab().save(out+"/Split.png");
  QStringList expectedQuick{"RL","IL","NEXT","FEXT","TDR"};
  for(int i=0;i<expectedQuick.size();++i){
   auto quick=w.findChild<QCheckBox*>(QString("quickMetric%1").arg(i));
   check(quick&&quick->text()==expectedQuick[i],"Compact Quick Analysis control labels");
  }
  auto compactSettings=w.findChild<QScrollArea*>("analysisSettingsPanel");
  check(compactSettings&&compactSettings->maximumHeight()<=74,"Compact analysis settings preserve more plot height");
  result["compact_quick_analysis_ui"]=true;
  result["compact_main_three_languages_984x688"]=true;

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
  widthDiagnostics(w,"table-font-18");
  assertCompactToolbar(w);
  check(w.table->verticalHeader()->defaultSectionSize()==34,"Table font row size");
  check(w.table->selectionModel()->selectedRows()==selection,"Font/focus changed selection");
  w.grab().save(out+"/Results.png");
  w.workspaceActions[0]->trigger();settle(w);
  widthDiagnostics(w,"returned-to-split");
  assertCompactToolbar(w);
  auto restored=w.analysisSplit->sizes();
  check(restored.size()==2&&std::abs(restored[0]-split[0])<=2,"Split proportions not restored");
  check(w.channelPanel->isVisible()&&w.settingsPanel->isVisible(),"Panels not restored");
  check(w.plot->snapshot.viewStart==1e9&&w.plot->snapshot.viewStop==4e9,"Table focus reset axes");
  for(int i=0;i<15;++i){w.setWorkspaceMode(1);w.setWorkspaceMode(2);w.setWorkspaceMode(0);}
  settle(w);
  check(values(w)==original&&w.lastJobs.size()==jobs&&settingsJson(w.lastSettings)==settings,"Layout altered analysis or limits");
  check(w.table->selectionModel()->selectedRows()==selection,"Repeated switching lost selection");
  auto before=w.analysisSplit->sizes();
  w.analysisSplit->setSizes({before[0]+60,std::max(1,before[1]-60)});settle(w);
  bool splitterMoved = w.analysisSplit->sizes()!=before;
  if(!splitterMoved) {
    // A splitter already at its top/bottom limit can only move the other way.
    w.analysisSplit->setSizes({std::max(1,before[0]-60),before[1]+60});
    settle(w);
    splitterMoved = w.analysisSplit->sizes()!=before;
  }
  if(!splitterMoved) {
    // Cocoa may clamp a requested 900 px test window to the much smaller
    // runner's available screen height. When both panes are at their legal
    // minimum heights the divider cannot move; that is not a splitter defect.
    const auto minHeight=[](QWidget *widget) {
      return std::max(widget->minimumHeight(),
                      widget->minimumSizeHint().height());
    };
    const int minTop=minHeight(w.analysisSplit->widget(0));
    const int minBottom=minHeight(w.analysisSplit->widget(1));
    std::cerr<<"SPLITTER CLAMP DIAGNOSTIC: window="<<w.height()
             <<", requested="<<size.height()<<", panes="<<before[0]
             <<","<<before[1]<<", minima="<<minTop<<","<<minBottom
             <<"\n";
    check(w.height()<size.height() &&
          before[0]+before[1]<=minTop+minBottom+3,
          "Vertical splitter not adjustable despite available space");
    result["splitter_screen_constrained"]=true;
  } else result["splitter_screen_constrained"]=false;
  w.settingsAction->setChecked(true);settle(w);
  check(w.quickButton->mapTo(w.channelPanel,QPoint(0,0)).y()>=0 && w.quickButton->mapTo(w.channelPanel,QPoint(0,w.quickButton->height())).y()<=w.channelPanel->height(),"Quick Analysis button exceeds channel viewport");
  check(w.settingsPanel->isVisible()&&w.height()<=size.height(),"Expanded settings exceed viewport");
  auto scroll=qobject_cast<QScrollArea*>(w.settingsPanel);check(scroll&&scroll->widget(),"Settings scroll container missing");
  w.grab().save(out+"/Settings.png");w.settingsAction->setChecked(false);settle(w);
  check(!w.analysisSplit->opaqueResize(),"Splitter triggers continuous redraw");
  stage("fullscreen transitions");
  widthDiagnostics(w,"before-fullscreen");
  assertCompactToolbar(w);
  auto normalGeometry=w.geometry();
  const bool offscreen=QGuiApplication::platformName().compare("offscreen",Qt::CaseInsensitive)==0;
  if(!offscreen){
   auto waitForWindow=[&](const std::function<bool()> &ready,const char *message){
     QElapsedTimer transition;transition.start();
     while(!ready()&&transition.elapsed()<5000){
       QApplication::processEvents(QEventLoop::AllEvents,20);QThread::msleep(5);
     }
     if(!ready()) widthDiagnostics(w,"transition-failed");
     if(!ready()) std::cerr << "WORKSPACE FULLSCREEN RESTORE DIAGNOSTIC: "
       << "requested=" << normalGeometry.width() << "x" << normalGeometry.height()
       << " actual=" << w.width() << "x" << w.height()
       << " fullscreen=" << w.isFullScreen()
       << " maximized=" << w.isMaximized()
       << " saved=" << w.fullscreenNormalGeometry.width()
       << "x" << w.fullscreenNormalGeometry.height()
       << " minimumHint=" << w.minimumSizeHint().width()
       << "x" << w.minimumSizeHint().height()
       << " minimumSize=" << w.minimumWidth()
       << "x" << w.minimumHeight()
       << " centralHint=" << (w.centralWidget() ? w.centralWidget()->minimumSizeHint().width() : -1)
       << "x" << (w.centralWidget() ? w.centralWidget()->minimumSizeHint().height() : -1)
       << std::endl;
     check(ready(),message);settle(w);
   };
   w.toggleFullscreen();waitForWindow([&]{return w.isFullScreen();},"Fullscreen enter");
   // The Cocoa full-screen Space transition is asynchronous. Qt can report
   // WindowFullScreen before macOS finishes its animation; immediately calling
   // showNormal() can be ignored by AppKit while that transition is in flight.
   // Preserve real native Cocoa coverage rather than treating it as skipped.
   if(QGuiApplication::platformName().compare("cocoa",Qt::CaseInsensitive)==0){
     stage("wait for native Cocoa fullscreen entry to settle");
     QElapsedTimer nativeTransition;nativeTransition.start();
     while(nativeTransition.elapsed()<1800){
       QApplication::processEvents(QEventLoop::AllEvents,20);
       QThread::msleep(10);
     }
     check(w.isFullScreen(),"Native fullscreen lost before exit request");
   }
   stage("request fullscreen exit");
   assertCompactToolbar(w);
   w.toggleFullscreen();waitForWindow([&]{return !w.isFullScreen()&&w.size()==normalGeometry.size();},"Fullscreen normal-size restoration");
   assertCompactToolbar(w);
  }else{
   result["fullscreen_headless_skipped"]=true;
  }
  stage("metric viewing and persistence");
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
  result["axes_selection_results_preserved"]=true;result["divider_adjustable"]=true;result["fullscreen_restored"]=!offscreen;
  result["metric_switch_without_analysis"]=true;result["passed"]=true;w.dirty=false;return result;
 }
 static void fileWorkflowQualification(const QString &project, const QString &out) {
   stage("012_48 file/workspace lifecycle");
   const auto root=QDir(out).absoluteFilePath("file-workflow");
   check(QDir().mkpath(root),"Cannot create workflow fixture directory");
   const auto fixture=[&](const QString &name, const QByteArray &data) {
     const auto path=QDir(root).filePath(name);
     QFile f(path); check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot write workflow fixture");
     check(f.write(data)==data.size(),"Short workflow fixture write"); f.close(); return path;
   };
   QByteArray touchstone="! Port 1 = U1-A X_N\n! Port 2 = U1-B X_P\n! Port 3 = J1-1 X_N\n! Port 4 = J2-1 X_P\n# GHz S RI R 50\n";
   for(int point=1;point<=2;++point){
     touchstone+=QByteArray::number(point)+" ";
     for(int i=0;i<16;++i) touchstone+=".1 0 ";
     touchstone+="\n";
   }
   const auto a=fixture("first.s4p",touchstone), b=fixture("skip.s4p",touchstone),
              c=fixture("last.s4p",touchstone), invalid=fixture("bad.s4p","invalid\n");
   const auto se=fixture("mixed-size.s2p","# GHz S RI R 50\n1 .1 0 .8 0 .7 0 .2 0\n2 .1 0 .8 0 .7 0 .2 0\n");
   MainWindow w, other;
   w.show(); settle(w);
   const auto otherCache=other.workspaceCache->path();
   const auto initialCache=w.workspaceCache->path();
   check(initialCache!=otherCache,"Workspace caches not isolated");
   auto *newAction=w.findChild<QAction*>("newWorkspaceAction");
   check(newAction!=nullptr,"File/New action missing");
   w.loadProject(project,false); settle(w);
   check(!w.revisions.empty(),"Cannot load project for New test");
   w.run({si::Metric::RL}); settle(w);
   check(!w.results.empty(),"Missing prior result for New test");
   const auto oldCache=w.workspaceCache->path();
   const auto oldCount=w.revisions.size();
   const auto oldResults=values(w);
   w.dirty=true;
   QMessageBox::StandardButton choice=QMessageBox::Cancel;
   int prompts=0, warnings=0;
   QTimer answers; answers.setInterval(10);
   QObject::connect(&answers,&QTimer::timeout,&w,[&]{
     auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
     if(!box)return;
     if(box->standardButtons().testFlag(QMessageBox::Discard)){
       ++prompts; if(auto *button=box->button(choice))button->click();
     }else if(auto *ok=box->button(QMessageBox::Ok)){++warnings;ok->click();}
   });
   answers.start(); newAction->trigger(); answers.stop();
   check(prompts==1 && w.dirty && w.workspaceCache->path()==oldCache &&
         w.revisions.size()==oldCount && values(w)==oldResults && QDir(oldCache).exists(),
         "Cancel New mutated workspace");
   // Explicit existing destination avoids native Save As automation.
   w.projectPath=root; choice=QMessageBox::Save; answers.start();
   newAction->trigger(); answers.stop();
   check(prompts==2 && warnings==1 && w.dirty && w.workspaceCache->path()==oldCache &&
         w.revisions.size()==oldCount && values(w)==oldResults,"Failed save lost workspace");
   const auto saved=QDir(root).filePath("saved.siproject");
   w.projectPath=saved; answers.start(); newAction->trigger(); answers.stop(); settle(w);
   check(prompts==3 && QFileInfo(saved).isFile() && !w.dirty && w.revisions.empty() &&
         w.results.empty() && w.lastJobs.empty() && w.lastRevisions.empty() &&
         w.plot->snapshot.curves.empty() && w.table->rowCount()==0 &&
         w.compareTable->rowCount()==0 && w.revisionBox->count()==0 &&
         w.analysisWindows->context().sources.isEmpty() && w.projectPath.isEmpty() &&
         w.projectName=="Untitled" && !QDir(oldCache).exists() &&
         QDir(otherCache).exists(),"New/save/cache reset failed");
   check(w.limitValues[0]->value()==-10 && w.limitValues[1]->value()==-3 &&
         w.limitValues[2]->value()==-20 && w.limitValues[3]->value()==-20 &&
         !w.tdrLimit->isChecked(),"New changed default limit contract");
   check(sparamview::preferences::workingDirectory()==root,"New lost saved working directory");
   const auto secondCache=w.workspaceCache->path();
   newAction->trigger(); settle(w);
   check(!QDir(secondCache).exists() && QDir(otherCache).exists(),"Repeated New cleanup failed");

   QStringList reviewed;
   int failures=0;
   bool cancelEveryMapping=false;
   QTimer mapping; mapping.setInterval(10);
   QObject::connect(&mapping,&QTimer::timeout,&w,[&]{
     auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
     if(!d)return;
     if(auto *box=qobject_cast<QMessageBox*>(d)){
       if(auto *ok=box->button(QMessageBox::Ok)){++failures;ok->click();}
       return;
     }
     if(d->objectName()=="signalTypeDetectionDialog"){
       auto *selector=d->findChild<QComboBox*>("signalTypeSelector");
       if(selector)selector->setCurrentIndex(selector->findData(d->windowTitle().contains("mixed-size")?"Single-ended":"Differential"));
       auto *buttons=d->findChild<QDialogButtonBox*>();
       if(buttons && buttons->button(QDialogButtonBox::Ok))buttons->button(QDialogButtonBox::Ok)->click();
     }else if(d->objectName()=="channelMappingDialog"){
       const auto title=d->windowTitle();
       reviewed<<title;
       if(cancelEveryMapping || title.contains("skip")){d->reject();return;}
       auto *grid=d->findChild<QTableWidget*>();
       if(!grid||grid->rowCount()!=1){d->reject();return;}
       grid->item(0,0)->setText(title.contains("mixed-size")?"MAP_SE":title.contains("first")?"MAP_A":"MAP_C");
       if(title.contains("last")){
         // Swap complete Near/Far differential pairs.
         for(int k=1;k<=2;++k){
           const auto value=grid->item(0,k)->text();
           grid->item(0,k)->setText(grid->item(0,k+2)->text());
           grid->item(0,k+2)->setText(value);
         }
       }
       auto *buttons=d->findChild<QDialogButtonBox*>();
       if(buttons && buttons->button(QDialogButtonBox::Save))buttons->button(QDialogButtonBox::Save)->click();
     }
   });
   mapping.start();w.openPaths({a,b,invalid,c,se});settle(w);mapping.stop();
   check(reviewed.size()==4 && reviewed[0].contains("first") && reviewed[1].contains("skip") &&
         reviewed[2].contains("last") && reviewed[3].contains("mixed-size") && failures==1,
         "Per-file mapping order/cancel/failure not respected");
   check(w.revisions.size()==3 && w.revisions[0].source==a && w.revisions[1].source==c &&
         w.revisions[2].source==se && w.revisions[2].cache->meta().ports==2 &&
         w.revisions[2].confirmed && w.revisions[2].channels.size()==1 && w.revisions[2].channels[0].name=="MAP_SE" &&
         w.revisions[0].confirmed && w.revisions[1].confirmed &&
         w.revisions[0].channels.size()==1 && w.revisions[1].channels.size()==1 &&
         w.revisions[0].channels[0].name=="MAP_A" && w.revisions[1].channels[0].name=="MAP_C" &&
         w.revisions[0].channels[0].nearP==w.revisions[1].channels[0].farP,
         "Independent mapping not preserved");
   // All individually cancelled batch entries must leave the current workspace untouched.
   const auto beforeCancelledBatch=w.revisions.size();
   const auto beforeDirectory=sparamview::preferences::workingDirectory();
   cancelEveryMapping=true;reviewed.clear();
   mapping.start();w.openPaths({a,c});settle(w);mapping.stop();
   check(reviewed.size()==2 && w.revisions.size()==beforeCancelledBatch &&
         sparamview::preferences::workingDirectory()==beforeDirectory,
         "All-cancelled batch registered a revision or changed preferences");
   cancelEveryMapping=false;
   w.revisionBox->setCurrentIndex(0);w.run({si::Metric::RL});settle(w);
   check(!w.results.empty() && !w.lastRevisions.empty(),"Expected old analysis snapshot");
   const auto oldName=w.revisions[0].name;
   w.invalidateRevisionResults(oldName);
   check(std::none_of(w.results.begin(),w.results.end(),[&](const si::Result &r){return r.revision==ss(oldName);}) &&
         std::none_of(w.lastJobs.begin(),w.lastJobs.end(),[&](const si::Job &j){return j.revision==ss(oldName);}) &&
         std::none_of(w.lastRevisions.begin(),w.lastRevisions.end(),[&](const Revision &r){return r.name==oldName;}),
         "Invalidated mapping retained stale report data");
   choice=QMessageBox::Discard; answers.start();newAction->trigger();answers.stop();settle(w);
   check(w.revisions.empty() && QFileInfo(a).isFile() && QFileInfo(b).isFile() &&
         QFileInfo(c).isFile() && QFileInfo(se).isFile() && QFileInfo(saved).isFile() && QDir(otherCache).exists(),
         "Discard New removed source or other workspace");
   // Isolate the work-directory preference from the existing layout suite.
   {auto p=sparamview::preferences::store();p.remove("workspace/v1/lastDirectory");p.sync();}
 }

 static void integratedAnalysisQualification(const QString &out) {
   using si::gui::Feature;
   using si::channel::Status;
   stage("012_49 integrated application workflow");
   QTemporaryDir fixture(QDir(out).absoluteFilePath("integration-XXXXXX"));
   check(fixture.isValid(),"Cannot create integration fixture");
   auto write=[&](const QString &name,const QByteArray &data) {
     const QString path=QDir(fixture.path()).filePath(name);
     QFile file(path);
     check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot write integration fixture");
     check(file.write(data)==data.size(),"Short integration fixture write");
     file.close();return path;
   };
   QJsonArray sources;
   // Two different single-ended, two-lane S4P revisions. Matrix order follows
   // the existing channel GUI fixture. Only forward through loss differs.
   const QJsonArray lanes{
     QJsonObject{{"id","victim"},{"name","Victim"},{"group","Fixture"},{"nearP",0},{"farP",1}},
     QJsonObject{{"id","aggressor"},{"name","Aggressor"},{"group","Fixture"},{"nearP",2},{"farP",3}}};
   for(int rev=0;rev<2;++rev) {
     QByteArray data="# GHz S RI R 50\n";
     for(int freq=0;freq<=12;++freq) {
       data+=QByteArray::number(freq);
       for(int response=0;response<4;++response)
         for(int stimulus=0;stimulus<4;++stimulus) {
           double amplitude=0;
           if(response==stimulus)amplitude=.05;
           if(response/2==stimulus/2 && response!=stimulus)
             amplitude=rev==0?.8:.5;
           if(response==0 && stimulus==2)amplitude=.04;
           if(response==1 && stimulus==2)amplitude=.08;
           data+=" "+QByteArray::number(amplitude,'g',17)+" 0";
         }
       data+="\n";
     }
     const QString name=QString("Integration_%1").arg(rev);
     const QString path=write(name+".s4p",data);
     sources.append(QJsonObject{{"name",name},{"source",path},{"channels",lanes},
       {"confirmed",true},{"signalType","Single-ended"},{"signalTypeReviewed",true}});
   }
   si::Settings cfg;
   cfg.startHz=1e9;cfg.stopHz=10e9;cfg.tdrStopSeconds=3e-9;
   const auto project=write("input.siproject",QJsonDocument(QJsonObject{
     {"format","SIAnalyzerProject"},{"schema",1},{"name","Integration"},
     {"settings",settingsJson(cfg)},{"revisions",sources},
     {"ui",QJsonObject{{"revision",0},{"group","Fixture"},
       {"selected",QJsonArray{"victim","aggressor"}},{"metric",0}}}}).toJson());
   MainWindow w;
   w.show();settle(w);
   // Fail visibly instead of hanging on an unexpected native modal dialog.
   QString unexpectedModal;
   QTimer modalGuard;modalGuard.setInterval(20);
   QObject::connect(&modalGuard,&QTimer::timeout,&w,[&]{
     if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
       unexpectedModal=dialog->windowTitle();
       if(auto *box=qobject_cast<QMessageBox*>(dialog))unexpectedModal+=": "+box->text();
       std::cerr<<"012_49 unexpected modal: "<<unexpectedModal.toStdString()<<std::endl;
       dialog->reject();
     }
   });
   modalGuard.start();
   stage("012_49 load absolute fixture");
   w.loadProject(project,false);settle(w);
   check(unexpectedModal.isEmpty(),"Unexpected modal during integration project load");
   check(w.revisions.size()==2 && w.revisions[0].confirmed && w.revisions[1].confirmed,
         "Integration project did not restore two confirmed mappings");
   auto &manager=*w.analysisWindows;
   check(manager.context().sources.size()==2 && !manager.action(Feature::Com) &&
         !manager.openWindow(Feature::Com),"MainWindow release menu/context contract");
   auto open=[&](Feature feature) {
     auto *action=manager.action(feature);
     check(action && action->isEnabled(),"MainWindow analysis action unavailable");
     action->trigger();
     auto *window=manager.existingWindow(feature);
     check(window && window->isWindow() && window->isVisible(),"Analysis window missing");
     return window;
   };
   auto drain=[&] {
     QElapsedTimer clock;clock.start();
     while(manager.scheduler().inFlight()!=0) {
       QApplication::processEvents(QEventLoop::AllEvents,10);
       QThread::msleep(1);
       check(clock.elapsed()<20000,"Integrated channel worker timed out");
     }
     QApplication::processEvents();
   };
   auto read=[&](const QString &path) {
     QFile file(path);check(file.open(QIODevice::ReadOnly),"Integrated export missing");
     return file.readAll();
   };
   auto *ild=open(Feature::Ild);
   auto *icn=open(Feature::Icn);
   auto *eye=open(Feature::Eye);
   check(ild!=icn && ild!=eye && icn!=eye,"Analysis windows share identity");
   auto *fit=ild->findChild<QComboBox*>("ildFitModel");
   auto *ildStop=ild->findChild<QDoubleSpinBox*>("ildStopGhz");
   auto *items=icn->findChild<QListWidget*>("icnAggressorList");
   auto *profile=eye->findChild<QComboBox*>("eyeProfile");
   auto *mode=eye->findChild<QComboBox*>("eyeMode");
   auto *baud=eye->findChild<QDoubleSpinBox*>("eyeBitRateGbps");
   auto *cap=eye->findChild<QDoubleSpinBox*>("eyeAnalysisCapGhz");
   check(fit && ildStop && items && profile && mode && baud && cap,
         "Integrated feature controls missing");
   fit->setCurrentIndex(0);ildStop->setValue(10);
   profile->setCurrentIndex(int(si::channel::EyeProfile::Custom));
   mode->setCurrentIndex(1);baud->setValue(4.5);cap->setValue(10);
   auto *reference=eye->findChild<QDoubleSpinBox*>("eyeReferenceGhz");
   check(reference && std::abs(reference->value()-2.25)<1e-9,
         "Integrated Eye Nyquist reference mismatch");
   const char *fields[]={"eyeTxLowMv","eyeTxHighMv","eyeTxSourceOhm",
     "eyeRxTerminationOhm","eyeRxThresholdMv","eyeRxMinimumMv","eyeRxMaximumMv"};
   const char *electrical[]={"0","400","50","50","180","-500","1000"};
   for(int i=0;i<7;++i) {
     auto *field=eye->findChild<QLineEdit*>(fields[i]);
     check(field!=nullptr,"Integrated Eye electrical control missing");
     field->setText(QString::fromLatin1(electrical[i]));
   }
   const std::vector<si::Metric> metrics{si::Metric::RL,si::Metric::IL,
     si::Metric::NEXT,si::Metric::FEXT,si::Metric::TDR};
   w.run(metrics);settle(w);
   for(auto metric:metrics)
     check(std::any_of(w.results.begin(),w.results.end(),[&](const si::Result &r){
       return r.metric==metric && !r.plotX.empty();}),"Legacy integrated metric missing");
   const auto legacy=values(w);
   auto exercise=[&](int rev) {
     check(items->count()==2,"Integrated ICN aggressor list missing");
     items->item(0)->setCheckState(Qt::Checked);
     check(ild->runButton()->isEnabled() && icn->runButton()->isEnabled() &&
           eye->runButton()->isEnabled(),"Integrated analysis Run disabled");
     ild->runButton()->click();icn->runButton()->click();eye->runButton()->click();
     drain();
     auto usable=[](Status s){return s==Status::Valid || s==Status::Warning;};
     check(usable(ild->status()) && usable(icn->status()) && usable(eye->status()),
           "Integrated numerical worker failed");
     const QString prefix=QDir(fixture.path()).filePath(QString("rev%1-").arg(rev));
     check(ild->saveIldCsv(prefix+"ild.csv") && icn->saveIcnCsv(prefix+"icn.csv") &&
           eye->saveEyeCsv(prefix+"eye.csv"),"Integrated CSV export failed");
     const auto expectedSha=qs(w.revisions[size_t(rev)].cache->meta().sha256).toUtf8();
     for(const auto &suffix:{"ild.csv","icn.csv","eye.csv"}) {
       const auto csv=read(prefix+QString::fromLatin1(suffix));
       check(csv.contains(expectedSha) && csv.contains(w.revisions[size_t(rev)].name.toUtf8()),
             "Export provenance did not match active revision");
     }
     const auto ildCsv=read(prefix+"ild.csv");
     const auto rows=ildCsv.split('\n');
     bool found=false;
     for(const auto &row:rows) {
       if(row.isEmpty() || row.startsWith('#') || row.startsWith("Frequency"))continue;
       const auto columns=row.split(',');
       bool ok=false;
       check(columns.size()==4,"Integrated ILD CSV schema");
       const double loss=columns[1].toDouble(&ok);
       check(ok && std::abs(loss+20*std::log10(rev==0?.8:.5))<1e-8,
             "Integrated ILD analytical through-loss mismatch");
       found=true;
     }
     check(found,"Integrated ILD CSV missing data");
   };
   stage("012_49 first revision analysis");
   exercise(0);
   check(values(w)==legacy,"New analysis altered legacy results");
   w.revisionBox->setCurrentIndex(1);settle(w);
   const auto forbidden=QDir(fixture.path()).filePath("stale.csv");
   check(!ild->saveIldCsv(forbidden) && !icn->saveIcnCsv(forbidden) &&
         !eye->saveEyeCsv(forbidden) && !QFileInfo::exists(forbidden),
         "Revision change left stale feature export enabled");
   stage("012_49 second revision analysis");
   exercise(1);
   check(values(w)==legacy,"Revision analysis altered legacy result set");
   // Existing immutable results and canonical CSV must survive language switches.
   const QString parityPrefix=QDir(fixture.path()).filePath("rev1-");
   const auto ildBefore=read(parityPrefix+"ild.csv"),icnBefore=read(parityPrefix+"icn.csv"),eyeBefore=read(parityPrefix+"eye.csv");
   const auto statusBefore=eye->status();
   const auto contextBefore=manager.context().generation;
   const auto rowOrderBefore=w.resultRows;
   check(w.table->rowCount()>0 && w.table->item(0,0),"Result tooltip fixture missing");
   w.table->selectRow(0);
   const auto selectionBefore=w.table->selectionModel()->selectedRows();
   for(const auto *lang:{"ko","ja","en"}) {
     auto *action=w.findChild<QAction *>(QString("uiLanguage_")+lang);
     check(action!=nullptr,"Missing parity language action");
     action->trigger();
     const auto language=sparamview::ui_language::fromCode(QString::fromLatin1(lang));
     check(w.resultRows==rowOrderBefore && w.table->selectionModel()->selectedRows()==selectionBefore,"Language switch changed result row order or selection");
     for(int row=0;row<w.table->rowCount();++row) {
       const auto &result=w.results.at(w.resultRows.at(size_t(row)));
       check(w.table->item(row,0)->toolTip().contains(sparamview::ui_language::menuText("Worst margin at ",language)),"Existing result tooltip not localized");
       check(w.table->item(row,2)->text()==sparamview::ui_language::menuText(qs(si::name(result.metric)),language),"Existing result metric not localized");
     }
     const auto *eyeStatus=eye->findChild<QLabel *>("analysisStatus");
     check(eyeStatus && eyeStatus->text().contains(sparamview::ui_language::menuText("Simulated only — Not measured, no BER or PHY compliance; ",language)),"Eye simulation disclaimer not localized");
     const auto *eyeMetrics=eye->findChild<QLabel *>("eyeMetrics");
     check(eyeMetrics && eyeMetrics->text().contains("\n"+sparamview::ui_language::menuText("Original",language)+"\n"),"Original DC method presentation not localized");
     check(profile->currentText()==sparamview::ui_language::menuText("Custom (user-qualified)",language),"Eye profile presentation not localized");
     check(values(w)==legacy && eye->status()==statusBefore && manager.context().generation==contextBefore &&
           manager.scheduler().inFlight()==0,"Language switch changed computed result/context");
     check(ild->saveIldCsv(parityPrefix+"ild.csv") && icn->saveIcnCsv(parityPrefix+"icn.csv") &&
           eye->saveEyeCsv(parityPrefix+"eye.csv"),"Localized CSV export failed");
     check(read(parityPrefix+"ild.csv")==ildBefore && read(parityPrefix+"icn.csv")==icnBefore &&
           read(parityPrefix+"eye.csv")==eyeBefore,"Language switch changed canonical CSV bytes");
     int featureIndex=0;
     for(auto *window:{ild,icn,eye}) {
       window->resize(984,688);
       QApplication::processEvents();
       check(window->width()<=984 && window->height()<=688,"Localized analysis window exceeds 1024x768 work area");
       for(const auto *name:{"analysisRun","analysisCancel","analysisCsv","analysisPng"}) {
         auto *button=window->findChild<QPushButton *>(name);
         check(button && button->isVisible() && window->rect().contains(QRect(button->mapTo(window,QPoint(0,0)),button->size())),
               "Localized analysis action clipped");
       }
       for(const auto *name:{window==eye?"eyeSettingsScroll":"analysisSettingsScroll","analysisPlotScroll"}) {
         auto *scroll=window->findChild<QScrollArea *>(name);
         check(scroll && scroll->isVisible() && scroll->viewport()->width()>0 && scroll->viewport()->height()>0,
               "Localized analysis scroll viewport inaccessible");
       }
       const auto png=QDir(out).filePath(QString("language_%1_%2.png").arg(QString::fromLatin1(lang)).arg(featureIndex++));
       check(window->grab().save(png),"Localized screen evidence save failed");
     }
   }
   // A bad output target must fail without modifying the workspace or source.
   check(!ild->saveIldCsv(fixture.path()) && values(w)==legacy,
         "Export failure damaged main analysis");
   const auto saved=QDir(fixture.path()).filePath("roundtrip.siproject");
   w.groupBox->setCurrentIndex(0);w.aggressorBox->setCurrentIndex(0);
   w.findChild<QAction *>("uiLanguage_ko")->trigger();
   w.projectPath=saved;w.saveProject();
   check(QFileInfo(saved).isFile() && !w.dirty,"Integrated project save failed");
   const auto savedUi=QJsonDocument::fromJson(read(saved)).object()["ui"].toObject();
   check(savedUi["group"].toString()=="All groups" && savedUi["aggressor"].toString()=="AUTO (same group)",
         "Localized project persisted translated sentinel");
   w.findChild<QAction *>("uiLanguage_en")->trigger();
   // Hold a real verified source snapshot inside the same scheduler used by
   // Eye. This lifetime probe is separate from the actual Eye runs above.
   // A bounded release guard prevents hangs even if an assertion throws.
   stage("012_49 held source snapshot retirement");
   struct Gate { std::atomic_bool acquired{false}, release{false}, readOk{false}; };
   auto gate=std::make_shared<Gate>();
   struct ReleaseGate {
     std::shared_ptr<Gate> gate;
     ~ReleaseGate(){gate->release.store(true);}
   } releaseGate{gate};
   {
     const auto &rev=w.revisions[1];
     const auto generation=manager.context().generation;
     check(eye->startWork([cache=rev.cache,mapping=rev.channels,
           name=ss(rev.name),generation,gate](const std::atomic_bool &,
           std::atomic_int &)->si::gui::WorkOutcome {
       auto snapshot=si::channel::SourceSnapshot::capture(cache,mapping,name,generation,generation);
       gate->acquired.store(true);
       QElapsedTimer hold;hold.start();
       while(!gate->release.load() && hold.elapsed()<15000) QThread::msleep(1);
       if(!gate->release.load())throw si::Error("Snapshot hold timed out");
       const auto trace=si::channel::extract(snapshot,"victim",si::Metric::IL);
       if(trace.frequencyHz.empty())throw si::Error("Retired source unreadable");
       gate->readOk.store(true);
       return {Status::Valid,QStringLiteral("Held snapshot lifetime probe")};
     }),"Could not start held snapshot worker");
   }
   QElapsedTimer acquire;acquire.start();
   while(!gate->acquired.load() && acquire.elapsed()<5000) {
     QApplication::processEvents();QThread::msleep(1);
   }
   check(gate->acquired.load(),"Worker did not acquire source snapshot");
   const auto retired=w.workspaceCache->path();
   w.newWorkspace();settle(w);
   check(unexpectedModal.isEmpty() && w.revisions.empty() &&
         manager.scheduler().inFlight()>0 && QDir(retired).exists(),
         "New removed cache before held source worker released");
   gate->release.store(true);drain();
   check(gate->readOk.load(),"Held worker could not read retired source");
   QElapsedTimer cleanup;cleanup.start();
   while(QDir(retired).exists() && cleanup.elapsed()<5000) {
     QApplication::processEvents();QThread::msleep(10);
   }
   check(w.revisions.empty() && w.results.empty() && manager.context().sources.isEmpty() &&
         !ild->saveIldCsv(forbidden) && !icn->saveIcnCsv(forbidden) &&
         !eye->saveEyeCsv(forbidden) && !QFileInfo::exists(forbidden) &&
         !QDir(retired).exists(),"New retained stale results or retired cache");
   stage("012_49 project reload");
   w.loadProject(saved,false);settle(w);
   check(unexpectedModal.isEmpty(),"Unexpected modal during integration project reload");
   check(w.revisions.size()==2 && w.revisionBox->currentIndex()==1 &&
         manager.context().sources.size()==2 &&
         manager.context().sources[1].mappingConfirmed,"Integrated project restore failed");
   w.run(metrics);settle(w);
   check(values(w)==legacy,"Legacy results changed after project roundtrip");
   exercise(1);
   check(unexpectedModal.isEmpty(),"Unexpected modal during integrated workflow");
   modalGuard.stop();
   w.dirty=false;
   ild->close();icn->close();eye->close();QApplication::processEvents();
   check(QFileInfo(saved).isFile(),"Workspace lifecycle removed saved project");
   {auto p=sparamview::preferences::store();p.remove("workspace/v1/lastDirectory");p.sync();}
   stage("012_49 integrated application workflow passed");
 }
 static void languageMenuQualification() {
  using namespace sparamview::preferences;
  {auto p=store();p.remove("ui/v1/language");p.sync();}
  auto verify=[](MainWindow &w, const QString &file, const QString &selected) {
   auto *menu=w.findChild<QMenu *>("uiLanguageMenu");
   check(menu && menu->title()=="Language","Language menu must remain English");
   const QStringList labels={"English","Korean","Japanese"};
   check(menu->actions().size()==3,"Language action count");
   int checked=0;
   for(int i=0;i<3;++i) {
    auto *a=menu->actions()[i];
    check(a->text()==labels[i],"Language action must remain English");
    if(a->isChecked()) {++checked;check(a->text()==selected,"Wrong language selected");}
   }
   check(checked==1,"Language choice must be exclusive");
   check(w.menuBar()->actions().front()->text()==file,"File menu localization");
  };
  {
   MainWindow w;
   verify(w,"File","English");
   auto *ko=w.findChild<QAction *>("uiLanguage_ko");
   auto *ja=w.findChild<QAction *>("uiLanguage_ja");
   auto *en=w.findChild<QAction *>("uiLanguage_en");
   check(ko && ja && en,"Missing stable language action");
   const auto before=QLocale().name();
   const double limit=w.limitValues[0]->value();
   auto *newAction=w.findChild<QAction *>("newWorkspaceAction");
   check(newAction!=nullptr,"Missing New action");
   const auto shortcut=newAction->shortcut();
   const int marker=w.markerBox->currentIndex(), performance=w.performanceBox->currentIndex();
   using si::gui::Feature;
   auto *ild=w.analysisWindows->openWindow(Feature::Ild);
   auto *icn=w.analysisWindows->openWindow(Feature::Icn);
   auto *eye=w.analysisWindows->openWindow(Feature::Eye);
   check(ild && icn && eye,"Analysis windows missing");
   const auto generation=w.analysisWindows->context().generation;
   auto *direction=eye->findChild<QComboBox *>("analysisDirection");
   auto *manual=eye->findChild<QPushButton *>("analysisRun");
   check(direction && manual,"Eye language controls missing");
   direction->setCurrentIndex(1);
   const auto originalStatus=eye->status();
   auto fixedLabels=[&](const QString &source,const QString &victim,const QString &cancel,const QString &csv,const QString &settings){
     for(auto *window:{ild,icn,eye}) {
       bool sourceFound=false,victimFound=false;
       for(auto *label:window->findChildren<QLabel *>()) {
         sourceFound|=label->text()==source;
         victimFound|=label->text()==victim;
       }
       check(sourceFound && victimFound,"Localized form label binding failed");
       check(window->findChild<QPushButton *>("analysisCancel")->text()==cancel &&
             window->findChild<QPushButton *>("analysisCsv")->text()==csv &&
             window->findChild<QToolButton *>("analysisSettingsToggle")->text()==settings,
             "Localized fixed button binding failed");
     }
   };
   auto glyphs=[&]{
     check(!qApp->property("sparamview.japaneseFontFamily").toString().isEmpty(),"Bundled Japanese font not loaded");
     QTextLayout layout(QString::fromUtf8("日本語 ソース 設定 カタカナ ひらがな"),eye->font());
     layout.beginLayout();auto line=layout.createLine();line.setLineWidth(10000);layout.endLayout();
     int count=0;
     for(const auto &run:layout.glyphRuns())for(auto index:run.glyphIndexes()){
       check(index!=0,"Japanese glyph fallback produced missing glyph");++count;
     }
     check(count>0,"Japanese glyph layout was empty");
   };
   glyphs();
   for(int i=0;i<100;++i) {
    ko->trigger();verify(w,QString::fromUtf8("파일"),"Korean");
    check(newAction->text()==QString::fromUtf8("새로 만들기"),"Korean New action");
    check(w.tabs->tabText(0)==QString::fromUtf8("그래프"),"Korean plot tab");
    check(w.quickButton->text()==QString::fromUtf8("빠른 분석"),"Korean quick analysis button");
    check(manual->text()==QString::fromUtf8("수동 실행"),"Korean open Eye Run Manual");
    check(direction->itemText(1)==QString::fromUtf8("역방향"),"Korean open Eye direction");
    fixedLabels(QString::fromUtf8("원본 / 리비전"),QString::fromUtf8("피간섭 채널"),QString::fromUtf8("취소"),QString::fromUtf8("CSV 내보내기"),QString::fromUtf8("설정"));
    check(QCoreApplication::translate("QPlatformTheme","Cancel")==QString::fromUtf8("취소"),"Korean framework dialog translation");
    ja->trigger();verify(w,QString::fromUtf8("ファイル"),"Japanese");
    check(newAction->text()==QString::fromUtf8("新規作成"),"Japanese New action");
    check(w.tabs->tabText(0)==QString::fromUtf8("グラフ"),"Japanese plot tab");
    check(w.quickButton->text()==QString::fromUtf8("クイック解析"),"Japanese quick analysis button");
    check(manual->text()==QString::fromUtf8("手動実行"),"Japanese open Eye Run Manual");
    check(direction->itemText(1)==QString::fromUtf8("逆方向"),"Japanese open Eye direction");
    fixedLabels(QString::fromUtf8("ソース / リビジョン"),QString::fromUtf8("ビクティムチャネル"),QString::fromUtf8("キャンセル"),QString::fromUtf8("CSVエクスポート"),QString::fromUtf8("設定"));
    check(QCoreApplication::translate("QPlatformTheme","Cancel")==QString::fromUtf8("キャンセル"),"Japanese framework dialog translation");
    en->trigger();verify(w,"File","English");
    check(newAction->text()=="New","English New action");
    fixedLabels("Source / Revision","Victim Channel","Cancel","Export CSV","Settings");
    check(QCoreApplication::translate("QPlatformTheme","Cancel")=="Cancel","English framework dialog translation");
    check(manual->text()=="Run Manual" && direction->currentIndex()==1 && eye->status()==originalStatus && w.analysisWindows->context().generation==generation,"Language changed analysis state");
   }
   check(QLocale().name()==before && w.limitValues[0]->value()==limit &&
         newAction->shortcut()==shortcut && !w.dirty &&
         w.markerBox->currentIndex()==marker && w.performanceBox->currentIndex()==performance,
         "Menu language mutated application state");
   ja->trigger();
   ild->close();icn->close();eye->close();QApplication::processEvents();
  }
  {MainWindow w;verify(w,QString::fromUtf8("ファイル"),"Japanese");}
  {auto p=store();p.setValue("ui/v1/language","unsupported");p.sync();}
  {MainWindow w;verify(w,"File","English");}
  {auto p=store();p.remove("ui/v1/language");p.sync();}
 }
 static void preferencesQualification(const QString &out) {
  using namespace sparamview::preferences;
  const double expected[] = {-10, -3, -20, -20};
  {
   MainWindow w;
   for (int i=0;i<4;++i) {
    check(w.limitValues[i]->value()==expected[i],"Fresh GUI limit mismatch");
    check(!w.limits[i]->isChecked(),"Fresh GUI limit unexpectedly enabled");
   }
   check(w.tolerance->value()==10 && !w.tdrLimit->isChecked(),"TDR symmetric tolerance default");
   w.syncSettings();
   const auto saved = settingsJson(w.settings);
   const auto roundtrip = settingsFromJson(saved);
   check(roundtrip.rl.constant==-10 && roundtrip.il.constant==-3 &&
         roundtrip.next.constant==-20 && roundtrip.fext.constant==-20,
         "Default GUI/model roundtrip");
   auto legacy = saved;
   auto limits = legacy["limits"].toArray();
   auto old = limits[0].toObject();
   old["constant"]=0; old["enabled"]=true;
   limits[0]=old; legacy["limits"]=limits;
   const auto loaded = settingsFromJson(legacy);
   check(loaded.rl.constant==0 && loaded.rl.enabled,"Explicit legacy settings not preserved");
  }
  const auto dir=QDir(out).absoluteFilePath(QStringLiteral("Preference 한글 space"));
  check(QDir().mkpath(dir),"Cannot create preference directory fixture");
  const auto path=QDir(dir).filePath("sample.s2p");
  QFile fixture(path);
  check(fixture.open(QIODevice::WriteOnly),"Cannot create preference file");
  fixture.close();
  rememberFile(path);
  check(workingDirectory()==dir && workingDirectory(true)==dir,"Working directory roundtrip");
  check(saveSuggestion("report.csv")==QDir(dir).filePath("report.csv"),"Save suggestion");
  rememberFile({});
  check(workingDirectory()==dir,"Cancelled path changed working directory");
  {auto p=store(); p.setValue("workspace/v1/lastDirectory",QDir(dir).filePath("missing"));p.sync();}
  check(usableDirectory(workingDirectory()) && workingDirectory()!=QDir(dir).filePath("missing"),
        "Missing directory fallback");
  {auto p=store(); p.setValue("workspace/v1/lastDirectory",42);p.sync();}
  check(usableDirectory(workingDirectory()),"Corrupt directory fallback");
  const QRect monitor(-1280, 20, 1280, 700);
  const auto centered=centeredNormalGeometry(QSize(900,500),monitor);
  check(centered==QRect(-1090,120,900,500),"Negative screen center");
  const auto clipped=centeredNormalGeometry(QSize(8000,4000),monitor);
  check(clipped==QRect(-1264,52,1248,636) && monitor.contains(clipped),"Monitor bounds");
  const auto odd=centeredNormalGeometry(QSize(901,501),monitor);
  check(odd==QRect(-1091,119,901,501),"Odd-sized window center");
  {auto p=store();p.remove("workspace/v1/windowNormalSize");p.remove("workspace/v1/windowMaximized");p.sync();}
  check(readWindowState().maximized,"Missing window state must maximize");
  {
   MainWindow w; w.showWithSavedWindowState();
   check(w.isMaximized(),"Fresh window not maximized");
   w.close();
  }
  writeWindowState({QSize(900,600),false});
  {const auto state=readWindowState();check(!state.maximized && state.normalSize==QSize(900,600),"Normal state roundtrip");}
  {
   MainWindow w;w.showWithSavedWindowState();
   check(!w.isMaximized() && !w.isMinimized(),"Normal state not restored");
   w.close();
  }
  writeWindowState({QSize(1000,700),true});
  {const auto state=readWindowState();check(state.maximized && state.normalSize==QSize(1000,700),"Maximized state roundtrip");}
  {auto p=store();p.setValue("workspace/v1/windowNormalSize","bad");p.sync();}
  check(readWindowState().maximized && !readWindowState().normalSize.isValid(),"Corrupt state fallback");
  {auto p=store();p.remove("workspace/v1/lastDirectory");p.remove("workspace/v1/windowNormalSize");p.remove("workspace/v1/windowMaximized");p.sync();}
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
 applyLightTheme(app);check(QLocale().decimalPoint()==".","Technical UI must use decimal points");QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");
#ifdef Q_OS_WIN
 QFont::insertSubstitution("Segoe UI","NanumGothic");
#else
 check(!app.styleSheet().contains("Segoe UI"),"Non-Windows stylesheet requires a native font");
#endif
 try{
  QSize size(argc>3?QString::fromLocal8Bit(argv[3]).toInt():1440,argc>4?QString::fromLocal8Bit(argv[4]).toInt():980);
  localizationCatalogQualification();
  WorkspaceProbe::languageMenuQualification();
  WorkspaceProbe::preferencesQualification(out);
  WorkspaceProbe::fileWorkflowQualification(QString::fromLocal8Bit(argv[1]),out);
  WorkspaceProbe::integratedAnalysisQualification(out);
  QJsonObject result;
  WorkspaceProbe::stage("construct MainWindow");
  {MainWindow w;WorkspaceProbe::stage("MainWindow constructed");applyCompactAnalysisSettings(w);w.installHeaderProjectButtons();result=WorkspaceProbe::run(w,QString::fromLocal8Bit(argv[1]),out,size);}
  {MainWindow w;WorkspaceProbe::persistence(w);}
  result["preferences_restored"]=true;result["012_47_preferences_qualified"]=true;result["012_48_file_workflow_qualified"]=true;result["012_49_integration_qualified"]=true;QFile f(out+"/Workspace.json");check(f.open(QIODevice::WriteOnly),"Cannot save QA");f.write(QJsonDocument(result).toJson());std::cout<<QJsonDocument(result).toJson().constData();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
