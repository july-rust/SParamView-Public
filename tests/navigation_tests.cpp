#include "app.hpp"
#include "settings_cases.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <vector>
// Report descriptive timings; shared CI runners are not real-time systems.
static QJsonObject timingSummary(std::vector<double> values) {
  std::sort(values.begin(),values.end());
  if(values.empty()) return {};
  const size_t n=values.size();
  return {{"samples",int(n)},{"median_ms",(values[(n-1)/2]+values[n/2])/2.0},
          {"p95_ms",values[size_t(std::ceil(.95*n))-1]},{"max_ms",values.back()}};
}
struct NavigationProbe {
 static bool running(MainWindow &w) {
#ifdef SI_NEW_NAVIGATION
  return w.busy || w.viewRunning;
#else
  return w.busy;
#endif
 }
 static void settle(MainWindow &w) {
  QElapsedTimer t;t.start();
  while(running(w)) {QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);if(t.elapsed()>120000)throw si::Error("View timeout");}
  QCoreApplication::processEvents();
 }
 static QJsonObject run(MainWindow &w,QString folder) {
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
  w.revisionBox->clear();
  for(int i=0;i<32;++i){Revision r;r.name=QString("Rev %1").arg(i);r.cache=cache;r.source=qs(si::utf8(file));r.channels={ch};r.confirmed=true;w.revisions.push_back(r);w.revisionBox->addItem(r.name);}
  w.rebuildChannels();w.settings.tdrStopSeconds=180e-9;w.settings.tdrTerminations={si::Termination::Reference,si::Termination::Resistor,si::Termination::Split,si::Termination::Open,si::Termination::Short};w.applySettings();
  QElapsedTimer t;t.start();w.run({si::Metric::TDR});settle(w);
  QJsonObject result{{"analysis_ms",t.nsecsElapsed()/1e6},{"results",int(w.results.size())},{"overlays",int(w.plot->snapshot.curves.size())},
                     {"cache_open_ms",cacheOpenMs},{"fixture_ports",4},{"fixture_frequency_points",8193},{"fixture_revisions",32}};
  // Measure result table rebuild with a stable, already-computed data set.
  std::vector<double> tableTimes;
  w.refreshResults();
  for(int i=0;i<10;++i){t.restart();w.refreshResults();QCoreApplication::processEvents();tableTimes.push_back(t.nsecsElapsed()/1e6);}
  result["result_table_refresh"]=timingSummary(tableTimes);
  auto original=w.results;bool disabled=false;QJsonArray zooms;
  QElapsedTimer heartbeatClock;heartbeatClock.start();qint64 last=0,gap=0;QTimer beat;
  QObject::connect(&beat,&QTimer::timeout,[&]{auto now=heartbeatClock.elapsed();gap=std::max(gap,now-last);last=now;});beat.start(5);
  for(int i=0;i<3;++i){double l=(i+1)*2e-9,r=l+120e-9;t.restart();w.plot->setView(l,r,80,120);w.zoom(l,r);disabled|=!w.plot->isEnabled();settle(w);zooms.append(t.nsecsElapsed()/1e6);}
  double l=0,r=0;
  for(int i=0;i<100;++i){l=i*.01e-9;r=l+100e-9;w.plot->setView(l,r,85,115);w.zoom(l,r);}
  settle(w);gap=std::max(gap,heartbeatClock.elapsed()-last);beat.stop();result["zoom_ms"]=zooms;result["disabled_during_zoom"]=disabled;result["heartbeat_max_gap_ms"]=double(gap);
  result["latest_range_retained"]=w.plot->snapshot.viewStart==l&&w.plot->snapshot.viewStop==r;
  result["y_range_retained"]=w.plot->snapshot.viewBottom==85&&w.plot->snapshot.viewTop==115;
#ifdef SI_NEW_NAVIGATION
  const auto center=graphArea(w.plot->rect(),w.plot->snapshot).center();
  QWheelEvent pixelWheel(center,QPointF(w.plot->mapToGlobal(center.toPoint())),
       QPoint(0,30),QPoint(),Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,false);
  QApplication::sendEvent(w.plot,&pixelWheel);
  const double pixelSpan=w.plot->snapshot.viewStop-w.plot->snapshot.viewStart;
  QWheelEvent angleWheel(center,QPointF(w.plot->mapToGlobal(center.toPoint())),
       QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,false);
  QApplication::sendEvent(w.plot,&angleWheel);
  const double mouseSpan=w.plot->snapshot.viewStop-w.plot->snapshot.viewStart;
  result["trackpad_fractional_zoom"]=std::abs(pixelSpan/(r-l)-std::pow(.8,.25))<1e-9 &&
      std::abs(mouseSpan/pixelSpan-.8)<1e-9 &&
      w.plot->snapshot.viewBottom==85&&w.plot->snapshot.viewTop==115;
#endif
  w.plot->resetView();w.plot->repaint();
#ifdef SI_NEW_NAVIGATION
  auto renders=w.plot->graphRenderCount();
#endif
  std::vector<double> cursorTimes;QElapsedTimer sample;
  t.restart();
  for(int i=0;i<30;++i){sample.start();auto p=graphArea(w.plot->rect(),w.plot->snapshot).center()+QPointF(i,0);QMouseEvent e(QEvent::MouseMove,p,p,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(w.plot,&e);w.plot->repaint();cursorTimes.push_back(sample.nsecsElapsed()/1e6);}
  result["cursor_30_moves_ms"]=t.nsecsElapsed()/1e6;
  result["cursor_move_and_paint"]=timingSummary(cursorTimes);
#ifdef SI_NEW_NAVIGATION
  result["cursor_graph_renders"]=int(w.plot->graphRenderCount()-renders);
  w.zoom(0,100e-9);
  const int selectedRow=w.resultRows.empty()?-1:std::min<int>(2,int(w.resultRows.size())-1);
  if(selectedRow>=0)w.table->selectRow(selectedRow);settle(w);
  result["selection_preserved"]=selectedRow>=0&&size_t(selectedRow)<w.resultRows.size()&&w.plot->snapshot.curves.size()==1&&w.plot->snapshot.curves.front().revision==w.results[w.resultRows[size_t(selectedRow)]].revision&&w.plot->snapshot.curves.front().termination==w.results[w.resultRows[size_t(selectedRow)]].termination;
  w.table->selectAll();settle(w);result["selected_overlay_bounded"]=w.plot->snapshot.curves.size()<=64;
  auto memo=si::tdrCacheStats();result["cache_misses"]=int(memo.misses);result["cache_hits"]=int(memo.hits);
#endif
  bool intact=original.size()==w.results.size();
  for(size_t i=0;i<original.size();++i)intact&=original[i].worst==w.results[i].worst&&original[i].worstX==w.results[i].worstX&&original[i].status==w.results[i].status;
  result["statistics_unchanged"]=intact;w.grab().save(folder+"/Navigation.png");w.dirty=false;return result;
 }
};
int main(int argc,char **argv){QApplication app(argc,argv);applyLightTheme(app);QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");QFont::insertSubstitution("Segoe UI","NanumGothic");
 try{checkSettingsParsing();QElapsedTimer windowTimer;windowTimer.start();MainWindow w;w.resize(1440,960);w.show();app.processEvents();const double windowShowMs=windowTimer.nsecsElapsed()/1e6;QString dir=argc>1?QString::fromLocal8Bit(argv[1]):"navigation-test";auto r=NavigationProbe::run(w,dir);r["settings_validation"]=true;r["window_construct_and_show_ms"]=windowShowMs;r["qt_platform"]=QGuiApplication::platformName();QFile json(dir+"/benchmark.json");if(!json.open(QIODevice::WriteOnly))throw si::Error("Benchmark output failed");json.write(QJsonDocument(r).toJson());std::cout<<QJsonDocument(r).toJson().constData();
#ifdef SI_NEW_NAVIGATION
 if(r["disabled_during_zoom"].toBool()||!r["latest_range_retained"].toBool()||!r["y_range_retained"].toBool()||!r["selection_preserved"].toBool()||!r["selected_overlay_bounded"].toBool()||r["cursor_graph_renders"].toInt()!=0||!r["statistics_unchanged"].toBool()||!r["trackpad_fractional_zoom"].toBool())return 1;
#endif
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
