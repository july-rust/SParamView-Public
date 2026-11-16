// 012_04: event-loop and worker lifecycle tests (no numerical engine needed).
#include "gui/analysis/analysis_windows.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QEvent>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <stdexcept>

using namespace si::gui;
using S = si::channel::Status;
namespace {
void check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
bool pumpUntil(const std::function<bool()> &predicate, int timeoutMs = 6000) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!predicate() && elapsed.elapsed() < timeoutMs) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
    QThread::msleep(2);
  }
  QCoreApplication::processEvents();
  return predicate();
}
AnalysisContext context(const QString &sha = QStringLiteral("fixture-sha-256"),
                        bool confirmed = true) {
  AnalysisContext result;
  result.activeSource = 0;
  SourceItem source;
  source.revision = QStringLiteral("Fixture Revision A");
  source.sha256 = sha;
  source.mappingDigest = QStringLiteral("physical-mapping-fixture");
  source.channels = {QStringLiteral("Victim A"), QStringLiteral("Aggressor B")};
  source.mappingConfirmed = confirmed;
  result.sources.append(source);
  return result;
}
AsyncWork slowWork(int loops = 90) {
  return [loops](const std::atomic_bool &cancel, std::atomic_int &progress) -> WorkOutcome {
    for (int i = 0; i < loops; ++i) {
      if (cancel.load()) return {S::Cancelled, QStringLiteral("Cancellation observed")};
      progress.store(i * 100 / loops);
      QThread::msleep(3);
    }
    return {S::Valid, QStringLiteral("Fixture-only completion; not a real analysis")};
  };
}
void drain(AsyncScheduler &scheduler) {
  check(pumpUntil([&] { return scheduler.inFlight() == 0; }),
        "Worker did not drain before timeout");
}
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    QMainWindow main;
    AnalysisWindowManager manager(&main);
    manager.installMenu();
    check(main.menuBar()->findChild<QMenu *>(QStringLiteral("channelAnalysisMenu")),
          "Analysis menu missing");
    for (int i = 0; i < featureCount; ++i) {
      auto *action = manager.action(static_cast<Feature>(i));
      check(action && !action->isEnabled(), "Unimplemented engine is exposed as ready");
    }

    manager.setContext(context());
    AnalysisWindow *windows[featureCount]{};
    for (int i = 0; i < featureCount; ++i) {
      auto feature = static_cast<Feature>(i);
      windows[i] = manager.openWindow(feature);
      check(windows[i] && windows[i]->isWindow() && windows[i]->isVisible(),
            "Analysis must open in a non-modal top-level window");
      check(windows[i]->runButton() && !windows[i]->runButton()->isEnabled(),
            "An unfinished numerical engine must not run");
      check(manager.openWindow(feature) == windows[i], "Duplicate analysis window created");
      for (int j = 0; j < i; ++j)
        check(windows[i] != windows[j], "Distinct analysis features share a window");
    }
    // The real menu actions stay disabled until a future engine registers.
    // Only the isolated test manager enables the action to check focus/reuse.
    manager.setEngineAvailable(Feature::Ild, true);
    manager.action(Feature::Ild)->trigger();
    check(manager.existingWindow(Feature::Ild) == windows[0],
          "Repeated Analysis action did not reuse the existing window");
    manager.setEngineAvailable(Feature::Ild, false);

    // The main event loop must remain responsive during a background job.
    bool timerFired = false;
    QTimer::singleShot(20, &main, [&] { timerFired = true; });
    check(windows[0]->startWork(slowWork()), "Test job failed to start");
    check(pumpUntil([&] { return timerFired; }, 1000),
          "A worker blocked the Qt event loop");
    windows[0]->cancelButton()->click();
    drain(manager.scheduler());
    check(windows[0]->status() == S::Cancelled,
          "Cancelled job must not publish a VALID result");

    // A completed result is invalidated on context and mapping transitions.
    check(windows[1]->startWork(slowWork(5)), "Short job failed to start");
    drain(manager.scheduler());
    check(windows[1]->status() == S::Valid, "Fixture job completion missing");
    manager.setContext(context(QStringLiteral("different-source-sha")));
    check(windows[1]->status() == S::Stale,
          "Context change must invalidate previously completed results");

    // A result from the older source is never published as the current result.
    check(windows[2]->startWork(slowWork()), "Stale test job did not start");
    manager.setContext(context(QStringLiteral("newer-source-sha")));
    drain(manager.scheduler());
    check(windows[2]->status() == S::Stale, "Older source result was not discarded");

    // Exceptions from worker threads are carried to the GUI without terminate.
    check(windows[3]->startWork([](const std::atomic_bool &,
                                   std::atomic_int &) -> WorkOutcome {
      throw std::runtime_error("synthetic engine failure");
    }), "Exception test job did not start");
    drain(manager.scheduler());
    check(windows[3]->status() == S::Error, "Worker exception status not propagated");
    check(windows[3]->startWork([](const std::atomic_bool &,
                                   std::atomic_int &) -> WorkOutcome {
      throw si::Cancelled();
    }),"Cancelled exception test did not start");
    drain(manager.scheduler());
    check(windows[3]->status()==S::Cancelled,
          "Cooperative cancellation exception reported as ERROR");


    // Deleting a window while its worker is alive must not use a dangling QObject.
    check(windows[0]->startWork(slowWork()), "Close-while-busy job did not start");
    QPointer<AnalysisWindow> closed(windows[0]);
    windows[0]->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    check(!closed, "Closed window was not deleted");
    drain(manager.scheduler());
    auto *reopened = manager.openWindow(Feature::Ild);
    check(reopened && reopened->isVisible(), "Closed analysis window did not reopen");

    // No silently valid result from unconfirmed mappings.
    manager.setContext(context(QStringLiteral("unknown"), false));
    check(!reopened->startWork(slowWork(1)), "Unconfirmed mapping accepted");
    check(reopened->status() == S::InvalidSource, "Invalid mapping not diagnosed");

    // An old project workspace must remain alive while a cancelled/retired
    // worker can still read its SourceSnapshot, then be removed on drain.
    manager.setContext(context());
    check(reopened->startWork(slowWork()), "Retired workspace fixture did not start");
    auto retired = std::make_shared<QTemporaryDir>();
    check(retired->isValid(), "Temporary project workspace could not be created");
    const QString retiredPath = retired->path();
    manager.retireWorkspace(retired);
    retired.reset();
    check(QDir(retiredPath).exists(), "Workspace removed while analysis worker was running");
    manager.scheduler().cancel(Feature::Ild);
    drain(manager.scheduler());
    check(pumpUntil([&] { return !QDir(retiredPath).exists(); }),
          "Retired workspace was not removed after workers drained");

    manager.setContext(context());
    for (int i = 0; i < featureCount; ++i) {
      auto *w = manager.existingWindow(static_cast<Feature>(i));
      check(w && w->isVisible(), "One feature close affected an unrelated window");
    }

    // 012_05 real channel fixture: verified cached Touchstone -> independent ILD
    // numerical worker -> GUI results, without fake completed jobs.
    QTemporaryDir ildDir;
    check(ildDir.isValid(), "ILD fixture temp directory");
    const auto snp=si::pathFromUtf8((ildDir.path()+QStringLiteral("/ild.s2p")).toStdString());
    {
      std::ofstream out(snp);
      out<<std::setprecision(17)<<"# GHz S RI R 50\n";
      for(int i=0;i<=16;++i) {
        const double x=double(i)/16.0;
        const double loss=1.5+2.0*std::sqrt(x)+3.0*x+0.25*x*x;
        const double v=std::pow(10.0,-loss/20.0);
        out<<i<<" 0 0 "<<v<<" 0 "<<v<<" 0 0 0\n";
      }
    }
    auto cache=si::Cache::open(snp,
      si::pathFromUtf8((ildDir.path()+QStringLiteral("/cache")).toStdString()),
      nullptr,true);
    si::Channel channel;
    channel.id="victim";channel.name="Victim";channel.nearP=0;channel.farP=1;
    AnalysisContext numericContext;
    numericContext.activeSource=0;
    SourceItem numericSource;
    numericSource.sha256=QString::fromStdString(cache->meta().sha256);
    numericSource.mappingDigest=QStringLiteral("confirmed-physical-map");
    numericSource.revision=QStringLiteral("ILD fixture revision");
    numericSource.mappingConfirmed=true;
    numericSource.channels={QStringLiteral("Victim")};
    numericSource.channelIds={QStringLiteral("victim")};
    numericContext.sources.append(numericSource);
    manager.setIldSourceProvider([cache,channel](int index)
      -> std::optional<IldSourceInput> {
      if(index!=0)return std::nullopt;
      return IldSourceInput{cache,{channel},"ILD fixture revision",
                            cache->meta().sha256,1,1};
    });
    manager.setContext(numericContext);
    auto *ild=manager.openWindow(Feature::Ild);
    check(manager.action(Feature::Ild)->isEnabled(), "ILD menu not enabled");
    check(ild->runButton()->isEnabled(), "Verified ILD Run disabled");
    ild->runButton()->click();
    drain(manager.scheduler());
    check(ild->status()==S::Valid, "Real ILD worker did not produce VALID result");
    check(ild->findChild<QWidget *>(QStringLiteral("ildResultGraph"))!=nullptr,
          "Actual ILD graph widget missing");
    auto *csv=ild->findChild<QPushButton *>(QStringLiteral("analysisCsv"));
    auto *png=ild->findChild<QPushButton *>(QStringLiteral("analysisPng"));
    check(csv && csv->isEnabled() && png && png->isEnabled(),
          "Verified ILD exports not enabled");
    const QString csvPath=ildDir.filePath(QStringLiteral("ild-result.csv"));
    const QString pngPath=ildDir.filePath(QStringLiteral("ild-result.png"));
    check(ild->saveIldCsv(csvPath),"Actual ILD CSV file export failed");
    check(ild->saveIldPng(pngPath),"Actual ILD PNG file export failed");
    QFile csvFile(csvPath);
    check(csvFile.open(QIODevice::ReadOnly),"ILD CSV cannot be read back");
    const QByteArray csvBytes=csvFile.readAll();
    check(csvBytes.contains("Frequency_GHz,IL_dB,Fitted_IL_dB,ILD_dB") &&
          csvBytes.contains("# source_sha256,") &&
          csvBytes.contains("# fitting_model,"),
          "CSV numerical/provenance columns missing");
    check(csvBytes.count('\n')>=32,"CSV numeric row count suspicious");
    QImage actualPng(pngPath);
    check(!actualPng.isNull() && actualPng.width()>200 &&
          actualPng.height()>200,"PNG not a usable rendered plot");

    numericContext.sources[0].sha256=QStringLiteral("stale-sha");
    manager.setContext(numericContext);
    check(ild->status()==S::Stale,"Context replacement failed to mark ILD STALE");
    check(!csv->isEnabled() && !png->isEnabled(),"Stale ILD allowed export");
    check(!ild->saveIldCsv(ildDir.filePath(QStringLiteral("stale.csv"))) &&
          !ild->saveIldPng(ildDir.filePath(QStringLiteral("stale.png"))),
          "STALE result was exported as current");
    // 012_06: real 4-port Touchstone, two mapped channels, separate NEXT/FEXT
    // paths -> generic NRZ ICN worker -> GUI -> CSV/PNG -> stale protection.
    const auto icnPath=si::pathFromUtf8(
      (ildDir.path()+QStringLiteral("/icn.s4p")).toStdString());
    {
      std::ofstream out(icnPath);
      out<<std::setprecision(17)<<"# GHz S RI R 50\n";
      for(int f=0;f<=8;++f) {
        out<<f;
        for(int sourcePort=0;sourcePort<4;++sourcePort)
          for(int responsePort=0;responsePort<4;++responsePort) {
            double value=0;
            if(sourcePort/2 != responsePort/2) value=0.08;
            else if(sourcePort!=responsePort) value=0.85;
            out<<" "<<value<<" 0";
          }
        out<<"\n";
      }
    }
    auto icnCache=si::Cache::open(icnPath,
       si::pathFromUtf8((ildDir.path()+QStringLiteral("/icn-cache")).toStdString()),
       nullptr,true);
    si::Channel icnVictim;
    icnVictim.id="victim";icnVictim.name="Victim";icnVictim.nearP=0;icnVictim.farP=1;
    si::Channel icnAggressor;
    icnAggressor.id="aggressor";icnAggressor.name="Aggressor";
    icnAggressor.nearP=2;icnAggressor.farP=3;
    AnalysisContext icnContext;
    icnContext.activeSource=0;
    SourceItem icnSource;
    icnSource.revision=QStringLiteral("ICN s4p fixture");
    icnSource.sha256=QString::fromStdString(icnCache->meta().sha256);
    icnSource.mappingDigest=QStringLiteral("icn-physical-map");
    icnSource.mappingConfirmed=true;
    icnSource.channels={QStringLiteral("Victim"),QStringLiteral("Aggressor")};
    icnSource.channelIds={QStringLiteral("victim"),QStringLiteral("aggressor")};
    icnContext.sources.append(icnSource);
    manager.setIcnSourceProvider([icnCache,icnVictim,icnAggressor](int idx)
        ->std::optional<IldSourceInput> {
      if(idx!=0) return std::nullopt;
      return IldSourceInput{icnCache,{icnVictim,icnAggressor},
         "ICN s4p fixture",icnCache->meta().sha256,1,1};
    });
    manager.setContext(icnContext);
    auto *icn=manager.openWindow(Feature::Icn);
    check(manager.action(Feature::Icn)->isEnabled() && icn->runButton()->isEnabled(),
          "ICN menu/run unavailable with verified source provider");
    auto *items=icn->findChild<QListWidget *>(QStringLiteral("icnAggressorList"));
    check(items && items->count()==2,"Expected NEXT and FEXT aggressor choices");
    items->item(0)->setCheckState(Qt::Checked);
    items->item(1)->setCheckState(Qt::Checked);
    icn->runButton()->click();
    check(icn->status()==S::NotEvaluated,
          "Multiple aggressors must require explicit RSS assumption");
    auto *rss=icn->findChild<QCheckBox *>(QStringLiteral("icnIndependentAssumption"));
    check(rss && !rss->isChecked(),"RSS assumption must not be pre-accepted");
    rss->setChecked(true);
    icn->runButton()->click();
    drain(manager.scheduler());
    check(icn->status()==S::Valid || icn->status()==S::Warning,
          "Real ICN did not produce qualified result");
    check(icn->findChild<QWidget *>(QStringLiteral("icnResultGraph"))!=nullptr,
          "Actual ICN result graph missing");
    auto *icnCsv=icn->findChild<QPushButton *>(QStringLiteral("analysisCsv"));
    auto *icnPng=icn->findChild<QPushButton *>(QStringLiteral("analysisPng"));
    check(icnCsv && icnCsv->isEnabled() && icnPng && icnPng->isEnabled(),
          "ICN CSV/PNG exports unavailable");
    const QString icnCsvPath=ildDir.filePath(QStringLiteral("icn-result.csv"));
    const QString icnPngPath=ildDir.filePath(QStringLiteral("icn-result.png"));
    check(icn->saveIcnCsv(icnCsvPath) && icn->saveIcnPng(icnPngPath),
          "Real ICN file export failed");
    QFile output(icnCsvPath);
    check(output.open(QIODevice::ReadOnly),"ICN CSV readback failed");
    const QByteArray content=output.readAll();
    check(content.contains("# total_rms_mV,") &&
          content.contains("# path_0_type,NEXT") &&
          content.contains("# path_1_type,FEXT") &&
          content.contains("Total_Cumulative_RMS_mV") &&
          content.count('\n')>=25,"ICN CSV columns or provenances missing");
    QImage icnImage(icnPngPath);
    check(!icnImage.isNull() && icnImage.width()>200 && icnImage.height()>200,
          "ICN PNG not an actual rendered plot");
    // Stale metadata and path selection must prevent all exports.
    icnContext.sources[0].sha256=QStringLiteral("changed-icn-sha");
    manager.setContext(icnContext);
    check(icn->status()==S::Stale && !icnCsv->isEnabled() && !icnPng->isEnabled(),
          "Stale ICN result remained exportable");
    check(!icn->saveIcnCsv(ildDir.filePath(QStringLiteral("stale-icn.csv"))) &&
          !icn->saveIcnPng(ildDir.filePath(QStringLiteral("stale-icn.png"))),
          "Stale ICN export was accepted");

    // 012_08: real SHA-qualified s2p -> SourceSnapshot -> phase-preserving
    // single-ended Eye worker -> graphs + CSV/PNG; no fabricated PHY voltage.
    numericContext.sources[0].sha256=QString::fromStdString(cache->meta().sha256);
    manager.setEyeSourceProvider([cache,channel](int index)
      -> std::optional<IldSourceInput> {
      if(index!=0)return std::nullopt;
      return IldSourceInput{cache,{channel},"ILD fixture revision",
                            cache->meta().sha256,1,1};
    });
    manager.setContext(numericContext);
    auto *eye=manager.openWindow(Feature::Eye);
    check(manager.action(Feature::Eye)->isEnabled() &&
          eye->runButton()->isEnabled(),"Verified Eye action unavailable");
    auto *profile=eye->findChild<QComboBox *>(QStringLiteral("eyeProfile"));
    auto *mode=eye->findChild<QComboBox *>(QStringLiteral("eyeMode"));
    auto *baud=eye->findChild<QDoubleSpinBox *>(QStringLiteral("eyeBitRateGbps"));
    auto *reference=eye->findChild<QDoubleSpinBox *>(QStringLiteral("eyeReferenceGhz"));
    auto *cap=eye->findChild<QDoubleSpinBox *>(QStringLiteral("analysisStopFrequency"));
    // Common stop-frequency spin gets an explicit ID in 012_08.
    if(!cap)cap=eye->findChild<QDoubleSpinBox *>(QStringLiteral("eyeAnalysisCapGhz"));
    check(profile && mode && baud && reference && cap,
          "Eye profile/bitrate/reference/cap controls missing");
    check(std::abs(baud->value()-4.5)<1e-9 &&
          std::abs(reference->value()-2.25)<1e-9 &&
          std::abs(cap->value()-10)<1e-9,
          "MIPI 4.5/2.25/10 example defaults were not installed");
    eye->runButton()->click();
    check(eye->status()==S::NotEvaluated,
          "MIPI preset without verified electrical inputs must fail closed");
    profile->setCurrentIndex(8); // Custom: single-ended s2p fixture
    mode->setCurrentIndex(1);
    baud->setValue(4.5);
    check(std::abs(reference->value()-2.25)<1e-9,
          "Reference was not coupled to bit rate");
    reference->setValue(1.5);
    check(std::abs(baud->value()-3.0)<1e-9,
          "Changing NRZ reference did not update bit rate");
    baud->setValue(4.5);
    cap->setValue(10);
    const QStringList electricalNames={
      QStringLiteral("eyeTxLowMv"),QStringLiteral("eyeTxHighMv"),
      QStringLiteral("eyeTxSourceOhm"),QStringLiteral("eyeRxTerminationOhm"),
      QStringLiteral("eyeRxThresholdMv"),QStringLiteral("eyeRxMinimumMv"),
      QStringLiteral("eyeRxMaximumMv")};
    const QStringList electricalValues={
      QStringLiteral("0"),QStringLiteral("400"),
      QStringLiteral("50"),QStringLiteral("50"),
      QStringLiteral("180"),QStringLiteral("-500"),
      QStringLiteral("1000")};
    for(int i=0;i<electricalNames.size();++i) {
      auto *field=eye->findChild<QLineEdit *>(electricalNames[i]);
      check(field!=nullptr,"Missing Eye electrical field");
      field->setText(electricalValues[i]);
    }
    // Wrong termination must not be converted silently into an Eye result.
    auto *rxTerm=eye->findChild<QLineEdit *>(QStringLiteral("eyeRxTerminationOhm"));
    rxTerm->setText(QStringLiteral("60"));
    eye->runButton()->click();
    drain(manager.scheduler());
    check(eye->status()==S::Error && !eye->saveEyeCsv(
        ildDir.filePath(QStringLiteral("bad-eye.csv"))),
        "Mismatched impedance produced a usable simulated Eye");
    rxTerm->setText(QStringLiteral("50"));
    eye->runButton()->click();
    drain(manager.scheduler());
    check(eye->status()==S::Warning,
          "Real single-ended s2p Eye must produce SIMULATED_ONLY WARNING");
    auto *eyeGraph=eye->findChild<QWidget *>(QStringLiteral("eyeResultGraph"));
    auto *metrics=eye->findChild<QLabel *>(QStringLiteral("eyeMetrics"));
    check(eyeGraph && metrics &&
          metrics->text().contains(QStringLiteral("Height")) &&
          metrics->text().contains(QStringLiteral("Width")) &&
          metrics->text().contains(QStringLiteral("Not Measured")),
          "Eye numerical display and caution missing");
    auto *eyeCsv=eye->findChild<QPushButton *>(QStringLiteral("analysisCsv"));
    auto *eyePng=eye->findChild<QPushButton *>(QStringLiteral("analysisPng"));
    check(eyeCsv && eyeCsv->isEnabled() && eyePng && eyePng->isEnabled(),
          "Verified simulated Eye exports unavailable");
    const QString eyeCsvPath=ildDir.filePath(QStringLiteral("eye-result.csv"));
    const QString eyePngPath=ildDir.filePath(QStringLiteral("eye-result.png"));
    check(eye->saveEyeCsv(eyeCsvPath) && eye->saveEyePng(eyePngPath),
          "Eye CSV/PNG export failed");
    QFile eyeRead(eyeCsvPath);
    check(eyeRead.open(QIODevice::ReadOnly),"Eye CSV not readable");
    const QByteArray eyeContent=eyeRead.readAll();
    check(eyeContent.contains("# source_sha256,") &&
          eyeContent.contains("# eye_height_mv,") &&
          eyeContent.contains("# reference_frequency_ghz,") &&
          eyeContent.contains("Time_ps,TX_mV,RX_mV") &&
          eyeContent.contains("Overlay_Index,Phase_UI,Received_mV") &&
          eyeContent.contains("SIMULATED_ONLY"),
          "Eye CSV missing numerical/provenance data");
    QImage eyeImage(eyePngPath);
    check(!eyeImage.isNull() && eyeImage.width()>200 && eyeImage.height()>200,
          "Eye PNG does not contain a real plot");
    numericContext.sources[0].sha256=QStringLiteral("source-hash-changed");
    manager.setContext(numericContext);
    check(eye->status()==S::Stale && !eyeCsv->isEnabled() &&
          !eyePng->isEnabled() &&
          !eye->saveEyeCsv(ildDir.filePath(QStringLiteral("stale-eye.csv"))),
          "STALE Eye remained exportable");

    // Real MIPI-profile UI with four physical ports and a differential THRU.
    // This is only an analytical s4p fixture, NOT a PHY-conformance oracle.
    const auto diffPath=si::pathFromUtf8(
      (ildDir.path()+QStringLiteral("/eye-diff.s4p")).toStdString());
    {
      std::ofstream out(diffPath);
      out<<"# GHz S RI R 50\n";
      for(int f=0;f<=12;++f) {
        out<<f;
        for(int sourcePort=0;sourcePort<4;++sourcePort)
          for(int responsePort=0;responsePort<4;++responsePort) {
            double v=0;
            if((sourcePort==0&&responsePort==2) ||
               (sourcePort==2&&responsePort==0) ||
               (sourcePort==1&&responsePort==3) ||
               (sourcePort==3&&responsePort==1))v=0.4;
            if((sourcePort==0&&responsePort==3) ||
               (sourcePort==3&&responsePort==0) ||
               (sourcePort==1&&responsePort==2) ||
               (sourcePort==2&&responsePort==1))v=-0.4;
            out<<" "<<v<<" 0";
          }
        out<<"\n";
      }
    }
    auto diffCache=si::Cache::open(diffPath,
      si::pathFromUtf8((ildDir.path()+QStringLiteral("/eye-diff-cache")).toStdString()),
      nullptr,true);
    si::Channel diffLane;
    diffLane.id="lane0";diffLane.name="Differential Lane 0";
    diffLane.nearP=0;diffLane.nearN=1;diffLane.farP=2;diffLane.farN=3;
    AnalysisContext diffContext;
    diffContext.activeSource=0;
    SourceItem diffSource;
    diffSource.revision=QStringLiteral("D-PHY fixture revision");
    diffSource.sha256=QString::fromStdString(diffCache->meta().sha256);
    diffSource.mappingDigest=QStringLiteral("differential-physical-map");
    diffSource.mappingConfirmed=true;
    diffSource.channels={QStringLiteral("Differential Lane 0")};
    diffSource.channelIds={QStringLiteral("lane0")};
    diffContext.sources.append(diffSource);
    manager.setEyeSourceProvider([diffCache,diffLane](int index)
        ->std::optional<IldSourceInput> {
      if(index!=0)return std::nullopt;
      return IldSourceInput{diffCache,{diffLane},"D-PHY fixture revision",
                            diffCache->meta().sha256,1,1};
    });
    manager.setContext(diffContext);
    profile->setCurrentIndex(0);
    check(mode->currentIndex()==2 && std::abs(baud->value()-4.5)<1e-9 &&
          std::abs(reference->value()-2.25)<1e-9 &&
          std::abs(cap->value()-10)<1e-9,
          "MIPI differential 4.5/2.25/10 GUI preset changed");
    const QStringList diffElectrical={
      QStringLiteral("-200"),QStringLiteral("200"),
      QStringLiteral("100"),QStringLiteral("100"),
      QStringLiteral("0"),QStringLiteral("-500"),
      QStringLiteral("500")};
    for(int i=0;i<electricalNames.size();++i) {
      auto *field=eye->findChild<QLineEdit *>(electricalNames[i]);
      field->setText(diffElectrical[i]);
    }
    eye->runButton()->click();
    drain(manager.scheduler());
    check(eye->status()==S::Warning,
          "MIPI labeled, physically differential s4p did not simulate");
    const QString diffCsvPath=ildDir.filePath(QStringLiteral("eye-diff-result.csv"));
    check(eye->saveEyeCsv(diffCsvPath),"Differential Eye CSV export failed");
    QFile diffRead(diffCsvPath);
    check(diffRead.open(QIODevice::ReadOnly) &&
          diffRead.readAll().contains("Sdd21"),
          "Differential Eye did not preserve Sdd21 mode provenance");
    // Requested upper cap cannot silently exceed the measured source.
    cap->setValue(15);
    eye->runButton()->click();
    drain(manager.scheduler());
    check(eye->status()==S::Error &&
          !eye->saveEyeCsv(ildDir.filePath(QStringLiteral("unmeasured-cap.csv"))),
          "Unmeasured Eye analysis cap did not fail closed");
    cap->setValue(10);

    manager.setEyeSourceProvider({});

    manager.setIcnSourceProvider({});
    manager.setIldSourceProvider({});
    std::cout << "012_04/012_05/012_06 QT GUI, ILD and ICN workflow PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "012_04 QT GUI/worker lifecycle FAIL: " << error.what() << '\n';
    return 1;
  }
}
