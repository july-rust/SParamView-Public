#include "app.hpp"
#include <iostream>
struct WorkspaceProbe {
 static void settle(MainWindow&w) {
  QElapsedTimer t;t.start();
  do { QApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);
   if(t.elapsed()>120000)throw si::Error("Capture timeout");
  }while(w.busy||w.viewRunning||t.elapsed()<200);
 }
 static void capture(MainWindow&w,QString project,QString out){
  w.resize(1920,1080);w.show();w.loadProject(project);settle(w);w.run(w.settings.quick);settle(w);
  if(w.results.empty())throw si::Error("No analysis to capture");
  w.table->selectRow(0);settle(w);
  if(!w.grab().save(out+"/SParamView_1.0.4_Split.png"))throw si::Error("Image save failed");
  w.setWorkspaceMode(1);w.plot->fitView();settle(w);
  if(!w.grab().save(out+"/SParamView_1.0.4_Graph.png"))throw si::Error("Image save failed");
  w.setWorkspaceMode(2);w.tableTextSize->setCurrentIndex(w.tableTextSize->findData(18));settle(w);
  if(!w.grab().save(out+"/SParamView_1.0.4_Results.png"))throw si::Error("Image save failed");w.dirty=false;
 }
};
int main(int argc,char**argv){QApplication app(argc,argv);if(argc!=3)return 2;app.setApplicationName("SParamView-Capture");applyLightTheme(app);
 QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/NanumGothic-Regular.ttf");QFont::insertSubstitution("Segoe UI","NanumGothic");
 try{MainWindow w;QDir().mkpath(QString::fromLocal8Bit(argv[2]));WorkspaceProbe::capture(w,QString::fromLocal8Bit(argv[1]),QString::fromLocal8Bit(argv[2]));return 0;}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
