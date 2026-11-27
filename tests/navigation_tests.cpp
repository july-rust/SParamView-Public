#include "app.hpp"
#include "settings_cases.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <vector>
#include <QTemporaryDir>
#include "../src/ui_language.hpp"
// Report descriptive timings; shared CI runners are not real-time systems.
static QJsonObject timingSummary(std::vector<double> values) {
  std::sort(values.begin(),values.end());
  if(values.empty()) return {};
  const size_t n=values.size();
  return {{"samples",int(n)},{"median_ms",(values[(n-1)/2]+values[n/2])/2.0},
          {"p95_ms",values[size_t(std::ceil(.95*n))-1]},{"max_ms",values.back()}};
}
// Independent synthetic files: no customer data or shared revision cache.
static QJsonArray cacheInputTimings() {
  QTemporaryDir dir;
  if(!dir.isValid()) throw si::Error("Cannot create benchmark directory");
  QJsonArray report;
  for(int ports : {2,4,12,36}) {
    const int points=ports<=4?8193:1025;
    const auto source=fp(dir.filePath(QString("synthetic.s%1p").arg(ports)));
    const auto cacheDir=fp(dir.filePath(QString("cache-%1").arg(ports)));
    std::ofstream out(source);
    out<<"# Hz S RI R 50\n";
    for(int i=0;i<points;++i){
      out<<static_cast<long long>(i)*1000000;
      for(int row=0;row<ports;++row)
        for(int col=0;col<ports;++col)
          out<<(row==col?" .1 0":(row^1)==col?" .6 0":" 0 0");
      out<<'\n';
    }
    out.close();
    if(!out) throw si::Error("Synthetic benchmark fixture write failed");
    QElapsedTimer timer;timer.start();
    auto cold=si::Cache::open(source,cacheDir);
    const double coldMs=timer.nsecsElapsed()/1e6;
    timer.restart();
    auto warm=si::Cache::open(source,cacheDir);
    const double warmMs=timer.nsecsElapsed()/1e6;
    if(cold->reused()||!warm->reused()||cold->meta().ports!=static_cast<unsigned>(ports)||
       cold->meta().points!=static_cast<unsigned>(points)||cold->frequencies()!=warm->frequencies())
      throw si::Error("Synthetic benchmark cache roundtrip failed");
    report.append(QJsonObject{{"ports",ports},{"points",points},
      {"source_bytes",double(si::fs::file_size(source))},{"empty_cache_open_ms",coldMs},
      {"reused_cache_open_ms",warmMs}});
  }
  return report;
}
// Exhaustive parity against the original catalog and unknown-key fallback.
static void checkCatalogLookupParity() {
  using namespace sparamview::ui_language;
  for(const auto &entry:catalogEntries()) {
    const auto key=QString::fromUtf8(entry.en);
    if(menuText(key,Language::English)!=key ||
       menuText(key,Language::Korean)!=QString::fromUtf8(entry.ko) ||
       menuText(key,Language::Japanese)!=QString::fromUtf8(entry.ja))
      throw si::Error("Translation index changed catalog contents");
  }
  const auto unknown=QStringLiteral("__01261_unknown_catalog_key__");
  for(const auto language:{Language::English,Language::Korean,Language::Japanese})
    if(menuText(unknown,language)!=unknown || !menuText(QString(),language).isEmpty())
      throw si::Error("Translation index changed unknown-key fallback");
}
struct NavigationProbe {
 static bool running(MainWindow &windowUnderTest) {
#ifdef SI_NEW_NAVIGATION
  return windowUnderTest.busy || windowUnderTest.viewRunning;
#else
  return windowUnderTest.busy;
#endif
 }
 static void settle(MainWindow &windowUnderTest) {
  QElapsedTimer benchmarkClock;benchmarkClock.start();
  while(running(windowUnderTest)) {QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);if(benchmarkClock.elapsed()>120000)throw si::Error("View timeout");}
  QCoreApplication::processEvents();
 }
 static QJsonObject run(MainWindow &windowUnderTest,QString folder) {
  QDir().mkpath(folder);auto file=fp(folder+"/input.s4p");std::ofstream out(file);
  out<<std::setprecision(17)<<"# Hz S RI R 50\n";
  for(int i=0;i<=8192;++i) {
   double f=i*20e9/8192;si::Complex a[4][4]{};
   for(int j=0;j<4;++j)a[j][j]=std::polar(.025,-2*3.141592653589793*f*.4e-9);
   a[0][2]=a[2][0]=std::polar(.85,-2*3.141592653589793*f*.8e-9);
   a[1][3]=a[3][1]=std::polar(.82,-2*3.141592653589793*f*.82e-9);
   out<<f;for(auto &row:a)for(auto z:row)out<<' '<<z.real()<<' '<<z.imag();out<<'\n';
  }out.close();QElapsedTimer cacheTimer;cacheTimer.start();
  auto cache=si::Cache::open(file,fp(folder+"/cache"));
  const double cacheOpenMs=cacheTimer.nsecsElapsed()/1e6;
  si::Channel ch;ch.id=ch.name="DIFF";ch.nearP=0;ch.nearN=1;ch.farP=2;ch.farN=3;
  windowUnderTest.revisionBox->clear();
  for(int i=0;i<32;++i){Revision r;r.name=QString("Rev %1").arg(i);r.cache=cache;r.source=qs(si::utf8(file));r.channels={ch};r.confirmed=true;windowUnderTest.revisions.push_back(r);windowUnderTest.revisionBox->addItem(r.name);}
  windowUnderTest.rebuildChannels();windowUnderTest.settings.tdrStopSeconds=180e-9;windowUnderTest.settings.tdrTerminations={si::Termination::Reference,si::Termination::Resistor,si::Termination::Split,si::Termination::Open,si::Termination::Short};windowUnderTest.applySettings();
  QElapsedTimer benchmarkClock;benchmarkClock.start();windowUnderTest.run({si::Metric::TDR});settle(windowUnderTest);
  QJsonObject result{{"analysis_ms",benchmarkClock.nsecsElapsed()/1e6},{"results",int(windowUnderTest.results.size())},{"overlays",int(windowUnderTest.plot->snapshot.curves.size())},
                     {"cache_open_ms",cacheOpenMs},{"fixture_ports",4},{"fixture_frequency_points",8193},{"fixture_revisions",32}};
  // Measure result table rebuild with a stable, already-computed data set.
  std::vector<double> tableTimes;
  windowUnderTest.refreshResults();
  for(int i=0;i<10;++i){benchmarkClock.restart();windowUnderTest.refreshResults();QCoreApplication::processEvents();tableTimes.push_back(benchmarkClock.nsecsElapsed()/1e6);}
  result["result_table_refresh"]=timingSummary(tableTimes);
  auto original=windowUnderTest.results;bool disabled=false;QJsonArray zooms;
  QElapsedTimer heartbeatClock;heartbeatClock.start();qint64 last=0,maximumHeartbeatGap=0;QTimer beat;
  QObject::connect(&beat,&QTimer::timeout,[&]{auto now=heartbeatClock.elapsed();maximumHeartbeatGap=std::max(maximumHeartbeatGap,now-last);last=now;});beat.start(5);
  for(int i=0;i<3;++i){double l=(i+1)*2e-9,r=l+120e-9;benchmarkClock.restart();windowUnderTest.plot->setView(l,r,80,120);windowUnderTest.zoom(l,r);disabled|=!windowUnderTest.plot->isEnabled();settle(windowUnderTest);zooms.append(benchmarkClock.nsecsElapsed()/1e6);}
  double l=0,r=0;
  for(int i=0;i<100;++i){l=i*.01e-9;r=l+100e-9;windowUnderTest.plot->setView(l,r,85,115);windowUnderTest.zoom(l,r);}
  settle(windowUnderTest);maximumHeartbeatGap=std::max(maximumHeartbeatGap,heartbeatClock.elapsed()-last);beat.stop();result["zoom_ms"]=zooms;result["disabled_during_zoom"]=disabled;result["heartbeat_max_gap_ms"]=double(maximumHeartbeatGap);
  result["latest_range_retained"]=windowUnderTest.plot->snapshot.viewStart==l&&windowUnderTest.plot->snapshot.viewStop==r;
  result["y_range_retained"]=windowUnderTest.plot->snapshot.viewBottom==85&&windowUnderTest.plot->snapshot.viewTop==115;
#ifdef SI_NEW_NAVIGATION
  const auto center=graphArea(windowUnderTest.plot->rect(),windowUnderTest.plot->snapshot).center();
  QWheelEvent pixelWheel(center,QPointF(windowUnderTest.plot->mapToGlobal(center.toPoint())),
       QPoint(0,30),QPoint(),Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,false);
  QApplication::sendEvent(windowUnderTest.plot,&pixelWheel);
  const double pixelSpan=windowUnderTest.plot->snapshot.viewStop-windowUnderTest.plot->snapshot.viewStart;
  QWheelEvent angleWheel(center,QPointF(windowUnderTest.plot->mapToGlobal(center.toPoint())),
       QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,false);
  QApplication::sendEvent(windowUnderTest.plot,&angleWheel);
  const double mouseSpan=windowUnderTest.plot->snapshot.viewStop-windowUnderTest.plot->snapshot.viewStart;
  result["trackpad_fractional_zoom"]=std::abs(pixelSpan/(r-l)-std::pow(.8,.25))<1e-9 &&
      std::abs(mouseSpan/pixelSpan-.8)<1e-9 &&
      windowUnderTest.plot->snapshot.viewBottom==85&&windowUnderTest.plot->snapshot.viewTop==115;
#endif
  windowUnderTest.plot->resetView();windowUnderTest.plot->repaint();
#ifdef SI_NEW_NAVIGATION
  auto renders=windowUnderTest.plot->graphRenderCount();
#endif
  std::vector<double> cursorTimes;QElapsedTimer sample;
  benchmarkClock.restart();
  for(int i=0;i<30;++i){sample.start();auto p=graphArea(windowUnderTest.plot->rect(),windowUnderTest.plot->snapshot).center()+QPointF(i,0);QMouseEvent e(QEvent::MouseMove,p,p,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(windowUnderTest.plot,&e);windowUnderTest.plot->repaint();cursorTimes.push_back(sample.nsecsElapsed()/1e6);}
  result["cursor_30_moves_ms"]=benchmarkClock.nsecsElapsed()/1e6;
  result["cursor_move_and_paint"]=timingSummary(cursorTimes);
#ifdef SI_NEW_NAVIGATION
  result["cursor_graph_renders"]=int(windowUnderTest.plot->graphRenderCount()-renders);
  windowUnderTest.zoom(0,100e-9);
  const int selectedRow=windowUnderTest.resultRows.empty()?-1:std::min<int>(2,int(windowUnderTest.resultRows.size())-1);
  if(selectedRow>=0)windowUnderTest.table->selectRow(selectedRow);settle(windowUnderTest);
  result["selection_preserved"]=selectedRow>=0&&size_t(selectedRow)<windowUnderTest.resultRows.size()&&windowUnderTest.plot->snapshot.curves.size()==1&&windowUnderTest.plot->snapshot.curves.front().revision==windowUnderTest.results[windowUnderTest.resultRows[size_t(selectedRow)]].revision&&windowUnderTest.plot->snapshot.curves.front().termination==windowUnderTest.results[windowUnderTest.resultRows[size_t(selectedRow)]].termination;
  windowUnderTest.table->selectAll();settle(windowUnderTest);result["selected_overlay_bounded"]=windowUnderTest.plot->snapshot.curves.size()<=64;
  auto memo=si::tdrCacheStats();result["cache_misses"]=int(memo.misses);result["cache_hits"]=int(memo.hits);
#endif
  bool intact=original.size()==windowUnderTest.results.size();
  for(size_t i=0;i<original.size();++i)intact&=original[i].worst==windowUnderTest.results[i].worst&&original[i].worstX==windowUnderTest.results[i].worstX&&original[i].status==windowUnderTest.results[i].status;
  result["statistics_unchanged"]=intact;windowUnderTest.grab().save(folder+"/Navigation.png");
#ifdef SI_NEW_NAVIGATION
  // Exercise the actual 64-curve boundary independently of the original
  // fixture, which has only five eligible termination curves per revision.
  if(windowUnderTest.plot->snapshot.curves.empty()) throw si::Error("Missing overlay prototype");
  auto prototype=windowUnderTest.plot->snapshot.curves.front();
  prototype.plotX={0,1e-9};prototype.plotY={100,100};prototype.plotComplete=true;
  auto saved=std::move(windowUnderTest.results);
  windowUnderTest.results.clear();
  for(int i=0;i<70;++i){
    auto r=prototype;r.channel=r.channelId="overlay-"+std::to_string(i);
    windowUnderTest.results.push_back(std::move(r));
  }
  windowUnderTest.refreshResults();windowUnderTest.refreshPlot();settle(windowUnderTest);
  bool boundary=windowUnderTest.plot->snapshot.curves.size()==64;
  for(size_t i=0;boundary&&i<64;++i)
    boundary=windowUnderTest.plot->snapshot.curves[i].channelId==windowUnderTest.results[i].channelId;
  windowUnderTest.table->selectAll();windowUnderTest.refreshPlot(true);settle(windowUnderTest);
  const auto selected=windowUnderTest.table->selectionModel()->selectedRows();
  boundary=boundary&&selected.size()==70&&windowUnderTest.plot->snapshot.curves.size()==64;
  for(size_t i=0;boundary&&i<64;++i)
    boundary=windowUnderTest.plot->snapshot.curves[i].channelId==
      windowUnderTest.results[windowUnderTest.resultRows[size_t(selected[int(i)].row())]].channelId;
  result["overlay_cap_order_preserved"]=boundary;
  // Large result table: measure actual refresh and verify pre-insertion
  // localization, including tooltip on every cell, with no canonical mutation.
  windowUnderTest.results.clear();
  for(int i=0;i<1000;++i){auto r=prototype;r.channel=r.channelId="table-"+std::to_string(i);windowUnderTest.results.push_back(std::move(r));}
  std::vector<double> largeTableTimes;int notifications=0;bool measuring=false;
  auto signal=QObject::connect(windowUnderTest.table->model(),&QAbstractItemModel::dataChanged,&windowUnderTest,[&]{if(measuring)++notifications;});
  bool tableOk=true;QJsonArray localeSamples;
  for(const auto *lang:{"ko","ja","en"}){
    auto *action=windowUnderTest.findChild<QAction *>(QString("uiLanguage_")+lang);
    if(!action)throw si::Error("Missing language action in large-table test");
    action->trigger();settle(windowUnderTest);
    for(int i=0;i<3;++i){measuring=true;benchmarkClock.restart();windowUnderTest.refreshResults();QCoreApplication::processEvents();largeTableTimes.push_back(benchmarkClock.nsecsElapsed()/1e6);measuring=false;}
    const auto language=sparamview::ui_language::current(&windowUnderTest);
    tableOk=tableOk&&windowUnderTest.table->rowCount()==1000&&windowUnderTest.resultRows.size()==1000;
    for(int row=0;tableOk&&row<windowUnderTest.table->rowCount();++row){
      const auto &r=windowUnderTest.results[windowUnderTest.resultRows[size_t(row)]];
      const auto expected=sparamview::ui_language::menuText(qs(si::name(r.metric)),language);
      const auto marker=sparamview::ui_language::menuText("Worst margin at ",language);
      const auto *channelItem=windowUnderTest.table->item(row,0),*metricItem=windowUnderTest.table->item(row,2);
      tableOk=channelItem&&metricItem&&channelItem->text()==qs(r.channel)&&metricItem->text()==expected;
      for(int col=0;tableOk&&col<windowUnderTest.table->columnCount();++col)
        tableOk=windowUnderTest.table->item(row,col)&&windowUnderTest.table->item(row,col)->toolTip().contains(marker);
    }
    localeSamples.append(QString::fromLatin1(lang));
  }
  // Ensure width invalidation handles longer text and later shrinking,
  // without capping or approximating the content scan.
  const int shortChannelWidth=windowUnderTest.table->columnWidth(0);
  for(auto &r:windowUnderTest.results) r.channel=std::string(48,'W');
  windowUnderTest.refreshResults();QCoreApplication::processEvents();
  const int longChannelWidth=windowUnderTest.table->columnWidth(0);
  for(auto &r:windowUnderTest.results) r.channel=r.channelId;
  windowUnderTest.refreshResults();QCoreApplication::processEvents();
  const int restoredChannelWidth=windowUnderTest.table->columnWidth(0);
  tableOk=tableOk&&longChannelWidth>shortChannelWidth&&restoredChannelWidth==shortChannelWidth;
  result["column_width_invalidation_preserved"]=longChannelWidth>shortChannelWidth&&restoredChannelWidth==shortChannelWidth;
  // Change every row twice: NG colors must appear and then disappear,
  // with changed values/tooltips and no stale result left in the table.
  std::vector<double> changedTableTimes;
  for(const auto *status:{"NG","OK"}) {
    const bool failed=QString::fromLatin1(status)=="NG";
    for(auto &r:windowUnderTest.results) {
      r.status=status;r.worst=failed?87.25:100.5;r.note=failed?"changed-failure-note":"changed-pass-note";
    }
    benchmarkClock.restart();windowUnderTest.refreshResults();QCoreApplication::processEvents();
    changedTableTimes.push_back(benchmarkClock.nsecsElapsed()/1e6);
    for(int row=0;tableOk&&row<windowUnderTest.table->rowCount();++row) {
      const auto &r=windowUnderTest.results[windowUnderTest.resultRows[size_t(row)]];
      const auto *statusCell=windowUnderTest.table->item(row,9),*valueCell=windowUnderTest.table->item(row,4);
      tableOk=statusCell&&valueCell&&statusCell->text()==qs(r.status)&&
        valueCell->text()==num(r.worst)&&
        statusCell->foreground().color()==QColor(failed?"#c6374a":"#00856c");
      for(int col=0;tableOk&&col<windowUnderTest.table->columnCount();++col) {
        const auto *cell=windowUnderTest.table->item(row,col);
        tableOk=cell&&cell->toolTip().contains(qs(r.note))&&
          (failed?cell->background().color()==QColor("#fff0f1"):cell->background().style()==Qt::NoBrush);
      }
    }
  }
  QObject::disconnect(signal);
  result["changed_result_table_refresh"]=timingSummary(changedTableTimes);
  result["large_result_table_refresh"]=timingSummary(largeTableTimes);
  result["large_result_table_rows"]=1000;
  result["large_result_table_data_changed_signals"]=notifications;
  result["large_result_table_locales"]=localeSamples;
  result["large_result_table_presentation_preserved"]=tableOk;
  windowUnderTest.results=std::move(saved);windowUnderTest.refreshResults();windowUnderTest.refreshPlot();settle(windowUnderTest);
#endif
  result["independent_cache_inputs"]=cacheInputTimings();
  windowUnderTest.dirty=false;return result;
 }
};
int main(int argc,char **argv){QApplication app(argc,argv);applyLightTheme(app);const int registeredFont=QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");QFont::insertSubstitution("Segoe UI","NanumGothic");
 try{checkSettingsParsing();checkCatalogLookupParity();QElapsedTimer windowTimer;windowTimer.start();MainWindow mainWindow;mainWindow.resize(1440,960);mainWindow.show();app.processEvents();const double windowShowMs=windowTimer.nsecsElapsed()/1e6;QString dir=argc>1?QString::fromLocal8Bit(argv[1]):"navigation-test";auto r=NavigationProbe::run(mainWindow,dir);r["settings_validation"]=true;r["catalog_lookup_parity"]=true;r["window_construct_and_show_ms"]=windowShowMs;r["qt_platform"]=QGuiApplication::platformName();r["benchmark_font_registered"]=registeredFont>=0;QFile json(dir+"/benchmark.json");if(!json.open(QIODevice::WriteOnly))throw si::Error("Benchmark output failed");json.write(QJsonDocument(r).toJson());std::cout<<QJsonDocument(r).toJson().constData();
#ifdef SI_NEW_NAVIGATION
 if(r["disabled_during_zoom"].toBool()||!r["latest_range_retained"].toBool()||!r["y_range_retained"].toBool()||!r["selection_preserved"].toBool()||!r["selected_overlay_bounded"].toBool()||r["cursor_graph_renders"].toInt()!=0||!r["statistics_unchanged"].toBool()||!r["trackpad_fractional_zoom"].toBool()||!r["overlay_cap_order_preserved"].toBool()||!r["large_result_table_presentation_preserved"].toBool())return 1;
#endif
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
