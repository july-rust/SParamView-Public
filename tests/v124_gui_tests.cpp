#include "app.hpp"
#include <fstream>
#include <iostream>
static void require(bool ok,const char *message){if(!ok)throw si::Error(message);}
struct WorkspaceProbe {
 static void settle(MainWindow &w) {
  QElapsedTimer t;t.start();
  while(w.busy||w.viewRunning){QApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(1);require(t.elapsed()<30000,"GUI operation timeout");}
  QApplication::processEvents();
 }
 static void setup(MainWindow &w,const QString &dir) {
  const auto source=dir+"/source.s4p";
  std::ofstream f(fp(source));f << "! Port[1] = U1-A DATAP0\n! Port[2] = U1-B DATAN0\n! Port[3] = J1-1 DATAP0\n! Port[4] = J1-2 DATAN0\n# Hz S RI R 50\n";
  for(int k=1;k<=4000;++k){f<<double(k)*1000000;for(int i=0;i<4;++i)for(int j=0;j<4;++j)f<<' '<<(i==j?.1:((i+2)%4==j?.8:.01))<<" 0";f<<'\n';}f.close();
  Revision r;r.source=source;r.name="fixture";r.cache=si::Cache::open(fp(source),fp(dir+"/cache"));
  r.channels={{"A","A","","Default",0,-1,2,-1},{"B","B","","Default",1,-1,3,-1}};
  r.confirmed=true;r.signalType="Single-ended";r.signalTypeReviewed=true;r.signalEvidence="analysis_snapshot";
  w.revisions={r};w.revisionBox->addItem(r.name);w.rebuildChannels();w.dirty=false;
 }
 static void run(const QString &test,const QString &dir) {
  MainWindow w;setup(w,dir);
  if(test=="reuse") {
   w.channels->item(1)->setCheckState(Qt::Unchecked);w.metricButtons[1]->click();settle(w);
   require(w.results.size()==2,"Initial A-only result count");
   w.channels->item(0)->setCheckState(Qt::Unchecked);w.channels->item(1)->setCheckState(Qt::Checked);
   w.metricButtons[1]->click();settle(w);
   require(std::all_of(w.results.begin(),w.results.end(),[](const auto&r){return r.channelId=="B";}),"Uncomputed B request reused A results");
   require(w.hasCurrentResult(si::Metric::IL),"Identical request cannot be reused");
   w.channels->item(0)->setCheckState(Qt::Checked);require(!w.hasCurrentResult(si::Metric::IL),"Adding channel did not change request");
   w.metricButtons[1]->click();settle(w);require(w.results.size()==4,"Added channel not calculated");
   w.search->setText("B");require(!w.hasCurrentResult(si::Metric::IL),"Search scope ignored");w.search->clear();
   w.groupBox->addItem("No matching group");w.groupBox->setCurrentText("No matching group");require(!w.hasCurrentResult(si::Metric::IL),"Group filter ignored");w.groupBox->setCurrentIndex(0);
   w.metricButtons[2]->click();settle(w);require(w.hasCurrentResult(si::Metric::NEXT),"AUTO NEXT cannot reuse");
   w.aggressorBox->setCurrentIndex(1);require(!w.hasCurrentResult(si::Metric::NEXT),"Aggressor ignored");w.aggressorBox->setCurrentIndex(0);
   w.setDirectionMode(2);require(w.hasCurrentResult(si::Metric::NEXT),"Display direction triggered recalculation");
   w.revisions[0].channels[0].farP=3;require(!w.hasCurrentResult(si::Metric::NEXT),"Changed physical mapping reused old result");
   w.revisions[0].signalEvidence="changed_live_evidence";w.revisions[0].signalType="Differential";
   w.limits[0]->setChecked(!w.limits[0]->isChecked());w.syncSettings();
   require(w.lastRevisions[0].signalEvidence=="analysis_snapshot"&&w.lastSettings.rl.enabled!=w.settings.rl.enabled,"Analysis snapshot was mutated by later edits");
   ReportOptions options;options.graphs=false;options.raw=false;si::Control control;
   writeReport(dir+"/snapshot.xlsx",w.results,w.lastRevisions,w.lastSettings,"snapshot",options,w.lastJobs,w.plot->snapshot,&control);
   QFile report(dir+"/snapshot.xlsx");require(report.open(QIODevice::ReadOnly),"Snapshot report missing");const auto bytes=report.readAll();
   require(bytes.contains("analysis_snapshot")&&!bytes.contains("changed_live_evidence"),"Excel exported current evidence instead of analysis snapshot");
  } else if(test=="dirty") {
   for(auto *box:{w.limits[0],w.limits[1],w.quickChecks[0],w.tdrLimit}){w.dirty=false;box->setChecked(!box->isChecked());require(w.dirty,"Checkbox change not tracked");}
   w.dirty=false;w.markerBox->setCurrentIndex((w.markerBox->currentIndex()+1)%4);require(w.dirty,"Marker mode not tracked");
   w.dirty=false;w.manualMarkers->setText("1, 2");require(w.dirty,"Manual markers not tracked");
   w.dirty=false;w.channels->item(0)->setCheckState(Qt::Unchecked);require(w.dirty,"Channel check state not tracked");
   w.projectPath=dir+"/state.siproject";w.saveProject();require(!w.dirty,"Save did not clear dirty state");
   w.loadProject(w.projectPath);settle(w);require(!w.dirty,"Restore marked project dirty");
  } else if(test=="project_changed") {
   w.projectPath=dir+"/state.siproject";w.saveProject();
   const auto source=fp(w.revisions[0].source);auto time=si::fs::last_write_time(source);
   std::ifstream input(source,std::ios::binary);std::string data((std::istreambuf_iterator<char>(input)),{});input.close();
   auto pos=data.find(".1",80000);require(pos!=std::string::npos,"Missing hash fixture datum");data[pos+1]='2';
   std::ofstream output(source,std::ios::binary);output<<data;output.close();si::fs::last_write_time(source,time);
   w.loadProject(w.projectPath);settle(w);
   require(!w.revisions[0].confirmed,"Same-size same-time source edit retained confirmed mapping");
   require(w.revisions[0].channels.empty(),"Old confirmed mapping repopulated changed source");
   require(!w.revisions[0].signalTypeReviewed,"Changed source retained signal review");
  } else if(test=="single_review") {
   auto &r=w.revisions[0];r.channels.clear();r.confirmed=false;r.signalTypeReviewed=false;r.signalType="Differential";r.candidates=si::suggestMapping(r.cache->meta());
   bool saw=false,bad=false;QTimer timer;
   QObject::connect(&timer,&QTimer::timeout,&w,[&]{for(auto *widget:QApplication::topLevelWidgets()){
    auto *d=qobject_cast<QDialog*>(widget);if(!d||!d->isVisible())continue;
    if(d->objectName()=="signalTypeDetectionDialog"){auto *s=d->findChild<QComboBox*>("signalTypeSelector");s->setCurrentIndex(s->findData("Single-ended"));d->accept();}
    else if(d->objectName()=="channelMappingDialog"){
     for(auto *b:d->findChildren<QPushButton*>())if(b->text()=="Suggest from Labels")b->click();
     auto *grid=d->findChild<QTableWidget*>();saw=true;
     for(int i=0;i<grid->rowCount();++i)if(grid->item(i,2)&&!grid->item(i,2)->text().isEmpty())bad=true;
     d->reject();
    }
   }});timer.start(20);w.editMapping();w.editMapping();timer.stop();
   require(saw&&!bad,"Suggest from Labels reenabled differential inference");
  } else if(test=="mixed") {
   for(const QString kind:{"single","diff","mixed"}) {
    MainWindow next;
    const auto file=dir+"/"+kind+".ts";
    std::ofstream f(fp(file));
    f<<"[Version] 2.1\n# Hz S RI R 50\n[Number of Ports] 4\n[Number of Frequencies] 1\n[Mixed-Mode Order] ";
    f<<(kind=="single"?"S1 S2 S3 S4":kind=="diff"?"D1,2 C1,2 D3,4 C3,4":"D1,2 C1,2 S3 S4");
    f<<"\n[Network Data]\n1";for(int i=0;i<16;++i)f<<" 0.1 0";f<<"\n[End]\n";f.close();
    QTimer dismiss;QObject::connect(&dismiss,&QTimer::timeout,&next,[]{if(auto*d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();});dismiss.start(10);
    next.openPaths({file});settle(next);dismiss.stop();
    require(next.revisions.size()==1,"Mixed metadata file not loaded");
    require(next.revisions[0].signalType==(kind=="single"?"Single-ended":kind=="diff"?"Differential":"Mixed"),"S/D/C classification mismatch");
    next.dirty=false;
   }
  } else if(test=="transition") {
   const auto original=dir+"/state.siproject";w.projectPath=original;w.saveProject();
   w.limits[0]->setChecked(!w.limits[0]->isChecked());require(w.dirty,"Transition fixture not dirty");
   int choice=QMessageBox::Cancel;bool cancelSave=false;QTimer responder;
   QObject::connect(&responder,&QTimer::timeout,&w,[&]{
    if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){
      if(box->standardButtons().testFlag(QMessageBox::Save))box->button(QMessageBox::StandardButton(choice))->click();
      else box->accept();
    } else if(cancelSave)if(auto *file=qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))file->reject();
   });responder.start(10);
   w.loadProject(original);settle(w);require(w.dirty,"Cancel lost changes");
   choice=QMessageBox::Save;cancelSave=true;w.projectPath.clear();
   w.loadProject(original);settle(w);require(w.dirty&&w.projectPath.isEmpty(),"Save dialog cancellation lost changes");
   cancelSave=false;w.projectPath=dir;w.loadProject(original);settle(w);require(w.dirty&&w.projectPath==dir,"Save failure discarded project");
   choice=QMessageBox::Discard;w.loadProject(original);settle(w);require(!w.dirty&&w.projectPath==original,"Explicit discard did not load target");responder.stop();
  } else if(test=="roundtrip") {
   auto &r=w.revisions[0];r.inferNamedDifferential=false;r.signalType="Single-ended";
   w.channels->item(0)->setCheckState(Qt::Unchecked);w.manualMarkers->setText("1, 2");
   w.projectPath=dir+"/state.siproject";w.saveProject();w.loadProject(w.projectPath);settle(w);
   require(!w.revisions[0].inferNamedDifferential,"Signal inference selection not restored");
   require(w.channels->item(0)->checkState()==Qt::Unchecked,"Channel selection not restored");
   require(w.settings.manualMarkersHz.size()==2&&!w.dirty,"Settings restore changed state");
   const auto source=fp(w.revisions[0].source);const auto moved=fp(dir+"/moved.s4p");si::fs::rename(source,moved);
   QFile json(w.projectPath);require(json.open(QIODevice::ReadOnly),"Project missing");auto object=QJsonDocument::fromJson(json.readAll()).object();json.close();
   auto revs=object["revisions"].toArray();auto rev=revs[0].toObject();rev["source"]="moved.s4p";rev["absoluteSource"]=qs(si::utf8(moved));revs[0]=rev;object["revisions"]=revs;
   require(json.open(QIODevice::WriteOnly),"Project update failed");json.write(QJsonDocument(object).toJson());json.close();
   w.loadProject(w.projectPath);settle(w);require(w.revisions[0].confirmed,"Same content at new path lost mapping");

   // A same-port-count label change must regenerate evidence, not retain mapping.
   QFile raw(qs(si::utf8(moved)));require(raw.open(QIODevice::ReadOnly),"Moved source missing");auto originalBytes=raw.readAll();raw.close();
   require(raw.open(QIODevice::WriteOnly),"Source fixture edit failed");raw.write(QByteArray(originalBytes).replace("DATAP0","DATAX0"));raw.close();
   w.loadProject(w.projectPath);settle(w);require(!w.revisions[0].confirmed&&w.revisions[0].channels.empty()&&!w.revisions[0].signalTypeReviewed,"Changed port labels retained confirmation");
   require(raw.open(QIODevice::WriteOnly),"Source fixture restore failed");raw.write(originalBytes);raw.close();
   w.loadProject(w.projectPath);settle(w);require(w.revisions[0].confirmed,"Original content did not restore saved mapping");
   // Older schema-1 files without optional fields remain readable.
   rev.remove("sourceSha256");rev.remove("inferNamedDifferential");revs[0]=rev;object["revisions"]=revs;
   require(json.open(QIODevice::WriteOnly),"Legacy project update failed");json.write(QJsonDocument(object).toJson());json.close();
   w.loadProject(w.projectPath);settle(w);require(w.revisions[0].confirmed&&!w.revisions[0].inferNamedDifferential,"Legacy schema-1 restoration failed");
   // A lower port count invalidates mapping but must not discard valid data.
   std::ofstream f(moved);f<<"# Hz S RI R 50\n1 .1 0 .8 0 .8 0 .1 0\n";f.close();
   auto smaller=fp(dir+"/smaller.s2p");si::fs::rename(moved,smaller);rev["source"]="smaller.s2p";rev["absoluteSource"]=qs(si::utf8(smaller));revs[0]=rev;object["revisions"]=revs;
   require(json.open(QIODevice::WriteOnly),"Small-port project update failed");json.write(QJsonDocument(object).toJson());json.close();
   w.loadProject(w.projectPath);settle(w);require(w.revisions[0].cache->meta().ports==2&&!w.revisions[0].confirmed&&w.revisions[0].channels.empty(),"Invalid stored mapping blocked valid source recovery");
  } else if(test=="workspace_switch") {
   const QString source=w.revisions[0].source;
   w.updateActiveTouchstoneTitle();
   require(w.windowTitle().contains(QFileInfo(source).fileName()),"Initial source title missing");
   const QString projectA=dir+"/project-A.siproject", projectB=dir+"/project-B.siproject";
   const QString other=dir+"/alternate-source.s4p";
   // The Windows build runs CTest twice in the same validation directory.
   // Only remove our own generated fixture, never an imported user source.
   QFile::remove(other);
   require(QFile::copy(source,other),"Cannot create second source");
   w.projectPath=projectA;w.saveProject();
   QFile a(projectA);require(a.open(QIODevice::ReadOnly),"Read project A");
   QJsonObject obj=QJsonDocument::fromJson(a.readAll()).object();a.close();
   auto array=obj["revisions"].toArray();
   auto rev=array[0].toObject();
   require(!rev.contains("cache"),"Serialized obsolete ephemeral cache field");
   rev["source"]=QFileInfo(other).fileName();
   rev["absoluteSource"]=other;array[0]=rev;obj["revisions"]=array;obj["name"]="Project B";
   QFile b(projectB);require(b.open(QIODevice::WriteOnly),"Write project B");
   b.write(QJsonDocument(obj).toJson());b.close();
   w.plot->snapshot.viewStart=1e9;w.plot->snapshot.viewStop=2e9;
   w.plot->snapshot.viewBottom=-40;w.plot->snapshot.viewTop=-5;
   w.compareTable->setColumnCount(1);w.compareTable->setRowCount(1);
   w.compareTable->setItem(0,0,new QTableWidgetItem("STALE"));
   w.loadProject(projectB);settle(w);
   require(w.projectPath==projectB&&w.revisions.size()==1&&w.revisions[0].source==other,
           "Project B did not replace A");
   require(w.compareTable->rowCount()==0&&w.compareTable->columnCount()==0,
           "Stale compare table was not cleared");
   require(std::isnan(w.plot->snapshot.viewStart)&&std::isnan(w.plot->snapshot.viewBottom),
           "Previous workspace axes survived replacement");
   require(w.results.empty()&&w.lastJobs.empty()&&w.lastRevisions.empty(),
           "Old analysis survived project replacement");
   require(w.windowTitle().contains("alternate-source.s4p"),"Title not switched to B");
   const QString cacheB=qs(si::utf8(w.revisions[0].cache->path()));
   const QString workspaceB=w.workspaceCache->path();
   require(cacheB.startsWith(workspaceB)&&QFileInfo::exists(cacheB),
           "Missing B cache in private workspace");
   w.loadProject(projectA);settle(w);
   require(w.projectPath==projectA&&w.revisions.size()==1&&w.revisions[0].source==source,
           "Failed B -> A project restore");
   require(!QFileInfo::exists(cacheB)&&!QDir(workspaceB).exists(),
           "Previous project cache survived replacement");
   require(w.windowTitle().contains(QFileInfo(source).fileName()),"Title not restored to A");
   const QString workspaceA=w.workspaceCache->path();
   const QString filename=QString(90,'X')+".s4p", longSource=dir+"/"+filename;
   QFile::remove(longSource);
   require(QFile::copy(source,longSource),"Long filename source creation failed");
   QTimer dismiss;
   QObject::connect(&dismiss,&QTimer::timeout,&w,[]{
     if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))
       dialog->reject();
   });
   dismiss.start(10);w.openPaths({longSource});settle(w);dismiss.stop();
   require(w.revisions.size()==2&&w.workspaceCache->path()==workspaceA,
           "Add Revision should preserve workspace and earlier cache");
   require(w.windowTitle().contains(filename)&&w.revisionBox->toolTip()==longSource,
           "Long file title or full-path tooltip missing");
   w.revisionBox->setCurrentIndex(0);
   require(w.windowTitle().contains(QFileInfo(source).fileName()),
           "Revision selection did not update title");
   const QString bad=dir+"/bad.siproject";
   QFile invalid(bad);require(invalid.open(QIODevice::WriteOnly),"Bad project fixture");
   invalid.write("invalid");invalid.close();
   const QString oldPath=w.projectPath,oldRoot=w.workspaceCache->path(),oldTitle=w.windowTitle();
   w.dirty=false;
   dismiss.start(10);w.loadProject(bad);dismiss.stop();
   require(w.projectPath==oldPath&&w.workspaceCache->path()==oldRoot &&
           w.revisions.size()==2&&w.windowTitle()==oldTitle,
           "Invalid project modified active workspace");
  } else if(test=="cleanup") {
   QString cacheDir, imported;
   {
    MainWindow second;
    cacheDir=second.cacheSession.path();
    w.projectPath=dir+"/cleanup-state.siproject";w.saveProject();
    require(QFileInfo::exists(w.projectPath),"Cleanup project fixture not saved");
    second.loadProject(w.projectPath);settle(second);
    require(second.revisions.size()==1&&second.revisions[0].cache,
            "Background GUI project import failed");
    imported=qs(si::utf8(second.revisions[0].cache->path()));
    require(QFileInfo::exists(imported),"Real GUI import did not create session cache");
    ReportOptions options;
    options.details=false;options.graphs=false;options.raw=false;
    si::Control control;
    writeReport(dir+"/cleanup-report.xlsx", {}, second.revisions,
                second.settings, "cleanup", options, {}, {}, &control, cacheDir);
    require(QFileInfo::exists(dir+"/cleanup-report.xlsx"),
            "Report write with session-scoped staging failed");
    require(QDir(cacheDir).entryList({"SParamView-report-*.tmp"}, QDir::Files).isEmpty(),
            "Finished report left an ephemeral staging file");
    second.dirty=false;
   }
   require(!QDir(cacheDir).exists()&&!QFileInfo::exists(imported),
           "GUI destruction left a cache directory or file");
   require(QFileInfo::exists(w.revisions[0].source),
           "GUI cache cleanup damaged the original Touchstone");
  } else throw si::Error("Unknown GUI case");
  w.dirty=false;
 }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setApplicationName("SParamView-v124-tests");
 if(argc<3)return 2;QDir().mkpath(argv[2]);
 try{WorkspaceProbe::run(argv[1],argv[2]);std::cout<<"PASS "<<argv[1]<<'\n';return 0;}
 catch(const std::exception&e){std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<'\n';return 1;}}
