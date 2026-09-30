#include "app.hpp"
#include "settings_cases.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
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
  }out.close();auto cache=si::Cache::open(file,fp(folder+"/cache"));
  si::Channel ch;ch.id=ch.name="DIFF";ch.nearP=0;ch.nearN=1;ch.farP=2;ch.farN=3;
  w.revisionBox->clear();
  for(int i=0;i<32;++i){Revision r;r.name=QString("Rev %1").arg(i);r.cache=cache;r.source=qs(si::utf8(file));r.channels={ch};r.confirmed=true;w.revisions.push_back(r);w.revisionBox->addItem(r.name);}
  w.rebuildChannels();w.settings.tdrStopSeconds=180e-9;w.settings.tdrTerminations={si::Termination::Reference,si::Termination::Resistor,si::Termination::Split,si::Termination::Open,si::Termination::Short};w.applySettings();
  QElapsedTimer t;t.start();w.run({si::Metric::TDR});settle(w);
  QJsonObject result{{"analysis_ms",t.nsecsElapsed()/1e6},{"results",int(w.results.size())},{"overlays",int(w.plot->snapshot.curves.size())}};
  auto original=w.results;bool disabled=false;QJsonArray zooms;
  QElapsedTimer heartbeatClock;heartbeatClock.start();qint64 last=0,gap=0;QTimer beat;
  QObject::connect(&beat,&QTimer::timeout,[&]{auto now=heartbeatClock.elapsed();gap=std::max(gap,now-last);last=now;});beat.start(5);
  for(int i=0;i<3;++i){double l=(i+1)*2e-9,r=l+120e-9;t.restart();w.plot->setView(l,r,80,120);w.zoom(l,r);disabled|=!w.plot->isEnabled();settle(w);zooms.append(t.nsecsElapsed()/1e6);}
  double l=0,r=0;
  for(int i=0;i<100;++i){l=i*.01e-9;r=l+100e-9;w.plot->setView(l,r,85,115);w.zoom(l,r);}
  settle(w);beat.stop();result["zoom_ms"]=zooms;result["disabled_during_zoom"]=disabled;result["heartbeat_max_gap_ms"]=double(gap);
  result["latest_range_retained"]=w.plot->snapshot.viewStart==l&&w.plot->snapshot.viewStop==r;
  result["y_range_retained"]=w.plot->snapshot.viewBottom==85&&w.plot->snapshot.viewTop==115;
  w.plot->resetView();w.plot->repaint();
#ifdef SI_NEW_NAVIGATION
  auto renders=w.plot->graphRenderCount();
#endif
  t.restart();
  for(int i=0;i<30;++i){auto p=graphArea(w.plot->rect(),w.plot->snapshot).center()+QPointF(i,0);QMouseEvent e(QEvent::MouseMove,p,p,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(w.plot,&e);w.plot->repaint();}
  result["cursor_30_moves_ms"]=t.nsecsElapsed()/1e6;
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
 try{checkSettingsParsing();MainWindow w;w.resize(1440,960);w.show();app.processEvents();QString dir=argc>1?QString::fromLocal8Bit(argv[1]):"navigation-test";auto r=NavigationProbe::run(w,dir);r["settings_validation"]=true;QFile json(dir+"/benchmark.json");if(!json.open(QIODevice::WriteOnly))throw si::Error("Benchmark output failed");json.write(QJsonDocument(r).toJson());std::cout<<QJsonDocument(r).toJson().constData();
#ifdef SI_NEW_NAVIGATION
 if(r["disabled_during_zoom"].toBool()||!r["latest_range_retained"].toBool()||!r["y_range_retained"].toBool()||!r["selection_preserved"].toBool()||!r["selected_overlay_bounded"].toBool()||r["cursor_graph_renders"].toInt()!=0||!r["statistics_unchanged"].toBool())return 1;
#endif
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
