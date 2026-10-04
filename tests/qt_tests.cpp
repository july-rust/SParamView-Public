#include "app.hpp"
#include "temp_directory.hpp"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
static int checks = 0;
static void expect(bool ok, const char *message) {
  ++checks;
  if (!ok)
    throw std::runtime_error(message);
}
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    QtWorkerPool pool;
    QThread *first = nullptr, *second = nullptr;
    pool.run(1, [&] { first = QThread::currentThread(); });
    pool.run(1, [&] { second = QThread::currentThread(); });
    expect(first == second && first != app.thread(),
           "Worker reuse outside GUI");
    for (size_t count : {size_t(1), size_t(2), size_t(8), size_t(1)}) {
      std::mutex mutex;
      std::condition_variable gate;
      size_t arrivals = 0;
      std::set<QThread *> identities;
      pool.run(count, [&] {
        std::unique_lock lock(mutex);
        identities.insert(QThread::currentThread());
        ++arrivals;
        gate.notify_all();
        if (!gate.wait_for(lock, std::chrono::seconds(5),
                           [&] { return arrivals == count; }))
          throw std::runtime_error("Worker rendezvous timeout");
      });
      expect(arrivals == count && identities.size() == count,
             "Bounded concurrency and reduction after Maximum");
    }
    std::atomic_int finished = 0;
    bool threw = false;
    try {
      pool.run(2, [&] {
        ++finished;
        throw std::runtime_error("expected failure");
      });
    } catch (const std::runtime_error &) {
      threw = true;
    }
    expect(threw && finished == 2, "Join before failure propagation");
    pool.run(1, [&] { ++finished; });
    expect(finished == 3, "Pool usable after failure");
    auto dir = si::fs::temp_directory_path() / "si_qt_tests";
    si::fs::create_directories(dir);
    TestDirectoryCleanup cleanup{dir};
    auto source = dir / "input.s2p";
    std::ofstream(source)
        << "# Hz S RI R 50\n1 .1 0 .8 0 .6 0 .2 0\n2 .1 0 .8 0 .6 0 .2 0\n";
    auto cache = si::Cache::open(source, dir / "cache");
    si::Channel channel;
    channel.id = channel.name = "data";
    channel.nearP = 0;
    channel.farP = 1;
    std::vector<si::Job> jobs(
        48, {cache, channel, std::nullopt, si::Metric::RL, "test"});
    si::WorkerRunner runner = [&](size_t n, const std::function<void()> &work) {
      pool.run(n, work);
    };
    si::Settings settings;
    si::Control cancel;
    cancel.progress = [&](double, const std::string &) {
      cancel.cancelled = true;
    };
    threw = false;
    try {
      si::runJobs(jobs, settings, &cancel, runner);
    } catch (const si::Cancelled &) {
      threw = true;
    }
    expect(threw, "Qt job cancellation");
    si::Control callbackFailure;
    callbackFailure.progress = [](double, const std::string &) {
      throw std::runtime_error("callback failure");
    };
    threw = false;
    try {
      si::runJobs(jobs, settings, &callbackFailure, runner);
    } catch (const std::runtime_error &) {
      threw = true;
    }
    expect(threw, "Progress exception safely propagates");
    for (int mode : {0, 1, 2}) {
      settings.performance = mode;
      auto results = si::runJobs(jobs, settings, nullptr, runner);
      expect(results.size() == jobs.size() &&
                 std::all_of(results.begin(), results.end(),
                             [](const auto &r) {
                               return std::abs(r.worst + 20) < 1e-12 &&
                                      r.direction == "Near -> Near";
                             }),
             "All modes preserve ordered numerical results");
    }
    int heartbeats = 0;
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++heartbeats; });
    heartbeat.start(1);
    QThreadPool coordinator;
    coordinator.setMaxThreadCount(1);
    settings.performance = 0;
    auto future = QtConcurrent::run(
        &coordinator, [&] { si::runJobs(jobs, settings, nullptr, runner); });
    QEventLoop loop;
    QFutureWatcher<void> watcher;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &loop,
                     &QEventLoop::quit);
    watcher.setFuture(future);
    loop.exec();
    future.waitForFinished();
    heartbeat.stop();
    expect(heartbeats >= 3, "GUI heartbeat during analysis");
    QImage picture(128, 64, QImage::Format_ARGB32);
    picture.fill(QColor("#008f91"));
    auto png = qs(si::utf8(dir / "roundtrip.png"));
    savePngFile(png, picture);
    QImage reread(png);
    expect(!reread.isNull() && reread.size() == picture.size() &&
               reread.pixel(64, 32) == picture.pixel(64, 32),
           "PNG committed pixel roundtrip");
    si::Control stopped;
    stopped.cancelled = true;
    threw = false;
    try {
      savePngFile(png, QImage(), &stopped);
    } catch (const si::Cancelled &) {
      threw = true;
    }
    expect(threw && QImage(png).pixel(64, 32) == picture.pixel(64, 32),
           "Cancelled PNG preserves previous file");
    PlotSnapshot snapshot;
    for (bool reverse : {false, true}) {
      snapshot.settings.reverse = reverse;
      for (auto metric : {si::Metric::RL, si::Metric::IL}) {
        snapshot.metric = metric;
        std::string suffix = metric == si::Metric::RL ? (reverse ? "22" : "11")
                                                      : (reverse ? "12" : "21");
        si::Result se, diff;
        se.parameter = "S" + suffix;
        diff.parameter = "Sdd" + suffix;
        snapshot.curves = {se, diff};
        auto label = graphParameterLabel(snapshot);
        expect(label.contains(qs(se.parameter) + " [dB]") &&
                   label.contains(qs(diff.parameter) + " [dB]"),
               "Mixed SE/differential parameter labels");
        snapshot.curves.clear();
        expect(graphParameterLabel(snapshot) == label,
               "Empty graph retains actual S parameter convention");
      }
    }
    snapshot.metric = si::Metric::TDR;
    si::Result good, limited, unsuitable;
    good.quality = "GOOD";
    limited.quality = "LIMITED";
    unsuitable.quality = "UNSUITABLE";
    snapshot.curves = {good, limited, unsuitable};
    expect(graphQualityLabel(snapshot) ==
               "TDR quality: GOOD 1  /  LIMITED 1  /  UNSUITABLE 1",
           "TDR graph quality counts");
    PlotSnapshot overlay;
    overlay.metric = si::Metric::RL;
    si::Result line, hump;
    line.parameter = hump.parameter = "S11";
    line.start = hump.start = 0;
    line.stop = hump.stop = 2;
    line.plotX = hump.plotX = {0, 1, 2};
    line.plotY = {-15, -15, -15};
    hump.plotY = {-5, -20, -5};
    overlay.curves = {line, hump};
    auto withoutMarker = graphImage(overlay, QSize(1200, 700));
    overlay.curves[0].peaks.push_back({0, -15, false});
    auto withMarker = graphImage(overlay, QSize(1200, 700));
    bool intact = true;
    for (int x = 200; x < 900; ++x)
      for (int y = 130; y < 600; ++y)
        intact =
            intact && (withoutMarker.pixel(x, y) == withMarker.pixel(x, y));
    expect(intact, "Adding remote marker cannot erase or fill other curves");
    QPalette simulatedDark;
    simulatedDark.setColor(QPalette::Window,Qt::black);
    simulatedDark.setColor(QPalette::Base,Qt::black);
    simulatedDark.setColor(QPalette::Text,Qt::white);
    app.setPalette(simulatedDark);
    applyLightTheme(app);
    for (auto group : {QPalette::Active,QPalette::Inactive,QPalette::Disabled}) {
      auto palette=app.palette();
      expect(palette.color(group,QPalette::Base)==QColor("#ffffff") &&
             palette.color(group,QPalette::Text).lightness()<140,
             "Dark host palette cannot leak into light popup text/background");
      expect(palette.color(group,QPalette::HighlightedText)==QColor("#ffffff") &&
             palette.color(group,QPalette::Highlight)==QColor("#006c70"),
             "Selected item foreground/background are specified together");
    }
    PlotWidget navigation;
    navigation.resize(1200,700);
    navigation.snapshot.metric=si::Metric::TDR;
    si::Result trace;
    trace.start=0;trace.stop=3e-9;trace.plotX={0,1e-9,3e-9};trace.plotY={100,105,100};
    trace.quality="LIMITED";trace.parameter="Sdd11";
    navigation.snapshot.curves={trace};
    auto full=graphXRange(navigation.snapshot);
    expect(std::abs(full.first + .1e-9) < 1e-20 &&
               std::abs(full.second - 1.5e-9) < 1e-20,
           "TDR default view keeps pre-zero margin and limits the initial tail");
    navigation.zoomBy(.5);
    auto zoomed=graphXRange(navigation.snapshot);
    expect(std::abs((zoomed.second-zoomed.first)/(full.second-full.first)-.5)<1e-12,
           "Toolbar zoom changes the visible X range");
    navigation.previousView();
    expect(graphXRange(navigation.snapshot)==full,"Previous view restores pre-zoom extent");
    navigation.setView(0,1e-9,90,110);
    expect(graphYRange(navigation.snapshot)==std::pair(90.,110.),"Manual Y axis overrides auto range");
    navigation.fitView();
    expect(graphXRange(navigation.snapshot)==std::pair(0.0,3e-9) &&
               std::isnan(navigation.snapshot.viewBottom),
           "Fit All restores the full computed TDR range and automatic Y axis");
    navigation.resetView();
    expect(graphXRange(navigation.snapshot)==full,
           "Reset restores the -0.1 ns to 1.5 ns initial TDR review window");
    PlotSnapshot terminationScale;
    terminationScale.metric = si::Metric::TDR;
    terminationScale.settings.tdrReflection = false;
    si::Result terminated;
    terminated.metric = si::Metric::TDR;
    terminated.termination = si::Termination::Open;
    terminated.targetOhm = 50;
    terminated.start = 0;
    terminated.stop = 99e-12;
    for (int i = 0; i < 100; ++i) {
      terminated.plotX.push_back(i * 1e-12);
      terminated.plotY.push_back(50.0 + 2.0 * std::sin(i * .17));
    }
    terminated.plotY.back() = 5000; // termination endpoint singularity
    terminationScale.curves = {terminated};
    auto terminationRange = graphYRange(terminationScale);
    expect(terminationRange.second < 100 && terminationRange.first > 0,
           "TDR termination autoscale ignores endpoint singularity for observation");
    terminationScale.fitAll = true;
    auto terminationFullRange = graphYRange(terminationScale);
    expect(terminationFullRange.second > 1000,
           "Explicit Fit All restores termination singularity visibility");
    terminationScale.fitAll = false;
    terminationScale.curves.front().termination = si::Termination::Reference;
    auto referenceRange = graphYRange(terminationScale);
    expect(referenceRange.second > 1000,
           "Reference TDR autoscale still honors full visible extrema");
    PlotSnapshot reviewScale;
    reviewScale.metric = si::Metric::TDR;
    si::Result reviewCurve;
    reviewCurve.metric = si::Metric::TDR;
    reviewCurve.termination = si::Termination::Reference;
    reviewCurve.targetOhm = 50;
    reviewCurve.start = 0;
    reviewCurve.stop = 20e-9;
    for (int i = 0; i <= 200; ++i) {
      const double x = i * .1e-9;
      reviewCurve.plotX.push_back(x);
      reviewCurve.plotY.push_back(i >= 25 && i <= 35 ? 62.0 : 50.0);
    }
    reviewScale.curves = {reviewCurve};
    auto reviewRange = graphXRange(reviewScale);
    expect(std::abs(reviewRange.first + .1e-9) < 1e-20 &&
               std::abs(reviewRange.second - 1.5e-9) < 1e-20,
           "TDR Review Fit uses the stable -0.1 ns to 1.5 ns initial window");
    reviewScale.curves.front().start = 1e-9;
    reviewScale.curves.front().stop = 3e-9;
    auto shiftedReviewRange = graphXRange(reviewScale);
    expect(std::abs(shiftedReviewRange.first - .9e-9) < 1e-20 &&
               std::abs(shiftedReviewRange.second - 2.5e-9) < 1e-20,
           "Nonzero TDR start keeps 0.1 ns pre-roll instead of forcing zero-based review");
    reviewScale.curves.front().start = 0;
    reviewScale.curves.front().stop = 20e-9;
    reviewScale.fitAll = true;
    auto fitAllRange = graphXRange(reviewScale);
    expect(std::abs(fitAllRange.first) < 1e-20 &&
               std::abs(fitAllRange.second - 20e-9) < 1e-20,
           "TDR Fit All restores the full computed time range");
    reviewScale.fitAll = false;
    reviewScale.viewStart = 2e-9;
    reviewScale.viewStop = 4e-9;
    expect(graphXRange(reviewScale) == std::pair(2e-9, 4e-9),
           "Manual TDR viewport overrides Review Fit without being reset");
    // TDR display interpolation is paint-only: it should create enough
    // screen vertices to remove the visibly angular line without overshooting
    // the original impedance envelope.
    std::vector<double> displayX{0, 1e-9, 2e-9, 3e-9, 4e-9, 5e-9};
    std::vector<double> displayY{50, 50, 64, 47, 55, 55};
    auto smoothSegments = tdrDisplaySegments(
        displayX, displayY, 0, 5e-9, 40, 70, QRectF(0, 0, 1000, 400), .08);
    expect(smoothSegments.size() == 1 &&
               smoothSegments.front().size() > qsizetype(displayX.size()),
           "Visible TDR interpolation adds paint resolution");
    bool insideEnvelope = true;
    for (const auto &point : smoothSegments.front())
      insideEnvelope = insideEnvelope && point.y() >= 0 && point.y() <= 400;
    expect(insideEnvelope,
           "Monotone TDR display interpolation cannot overshoot source envelope");

    // A long flat/dummy tail must collapse in pixel space instead of receiving
    // the same dense point allocation as the electrically interesting edge.
    std::vector<double> tailX, tailY;
    for (int i = 0; i <= 40; ++i) {
      tailX.push_back(i * 0.25e-9);
      tailY.push_back(i < 5 ? 50.0 + i * 3.0 : 62.0);
    }
    auto tailSegments = tdrDisplaySegments(
        tailX, tailY, 0, 10e-9, 40, 70, QRectF(0, 0, 1200, 400), .12);
    expect(tailSegments.size() == 1 && tailSegments.front().size() < 80,
           "Flat TDR dummy tail is not globally oversampled");
    navigation.setBoxZoom(true);
    auto area=graphArea(navigation.rect(),navigation.snapshot);
    QPointF a(area.left()+area.width()*.2,area.top()+area.height()*.2);
    QPointF b(area.left()+area.width()*.6,area.top()+area.height()*.8);
    QMouseEvent press(QEvent::MouseButtonPress,a,a,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&navigation,&press);
    QMouseEvent move(QEvent::MouseMove,b,b,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&navigation,&move);
    QMouseEvent release(QEvent::MouseButtonRelease,b,b,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&navigation,&release);
    auto boxRange=graphXRange(navigation.snapshot);
    expect(std::abs((boxRange.second-boxRange.first)/(full.second-full.first)-.4)<1e-12,
           "Mouse box zoom uses the actual TDR plot rectangle");
    navigation.setBoxZoom(false);
    const auto beforePan=graphXRange(navigation.snapshot);
    QApplication::sendEvent(&navigation,&press);
    QApplication::sendEvent(&navigation,&move);
    QApplication::sendEvent(&navigation,&release);
    const auto afterPan=graphXRange(navigation.snapshot);
    expect(afterPan.first<beforePan.first && std::abs((afterPan.second-afterPan.first)-(beforePan.second-beforePan.first))<1e-20,
           "Mouse pan moves the view without changing its span");
    navigation.fitView();
    const auto originalX=graphXRange(navigation.snapshot);
    const auto originalY=graphYRange(navigation.snapshot);
    auto wheel=[&](Qt::KeyboardModifiers modifiers) {
      const auto center=graphArea(navigation.rect(),navigation.snapshot).center();
      QWheelEvent event(center,center,QPoint(),QPoint(0,120),Qt::NoButton,
                        modifiers,Qt::NoScrollPhase,false);
      QApplication::sendEvent(&navigation,&event);
    };
    wheel(Qt::NoModifier);
    auto wx=graphXRange(navigation.snapshot);
    expect(std::abs((wx.second-wx.first)/(originalX.second-originalX.first)-.8)<1e-12 &&
           graphYRange(navigation.snapshot)==originalY,"Wheel zooms X only");
    wheel(Qt::ControlModifier);
    auto wy=graphYRange(navigation.snapshot);
    expect(graphXRange(navigation.snapshot)==wx &&
           std::abs((wy.second-wy.first)/(originalY.second-originalY.first)-.8)<1e-12,
           "Control wheel zooms Y without moving X");
    wheel(Qt::ShiftModifier);
    auto sxy=graphXRange(navigation.snapshot);auto sy=graphYRange(navigation.snapshot);
    expect(std::abs((sxy.second-sxy.first)/(wx.second-wx.first)-.8)<1e-12 &&
           std::abs((sy.second-sy.first)/(wy.second-wy.first)-.8)<1e-12,
           "Shift wheel zooms both axes");
    QKeyEvent home(QEvent::KeyPress,Qt::Key_Home,Qt::NoModifier);
    QApplication::sendEvent(&navigation,&home);
    expect(graphXRange(navigation.snapshot)==originalX && graphYRange(navigation.snapshot)==originalY,
           "Home key resets both axes");
    navigation.show(); navigation.invalidateGraph(); app.processEvents();
    int resizeRequests = 0;
    navigation.rangeChanged = [&](double, double) { ++resizeRequests; };
    navigation.snapshot.curves.front().plotComplete = false;
    navigation.resize(1500, 700);
    app.processEvents();
    expect(resizeRequests >= 1,
           "TDR resize reconstructs reduced viewport detail");
    navigation.snapshot.curves.front().plotComplete = true;
    auto rasterizations=navigation.graphRenderCount();
    for (int i=0;i<20;++i) {
      auto point=graphArea(navigation.rect(),navigation.snapshot).center()+QPointF(i,0);
      QMouseEvent event(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
      QApplication::sendEvent(&navigation,&event); app.processEvents();
    }
    expect(navigation.graphRenderCount()==rasterizations,"Cursor movement rerasterized static traces");
    QPolygonF dense;
    for(int i=0;i<1000;++i) dense.push_back({double(i),30*std::sin(i*.02)+(i==503?50.:0.)});
    auto reduced=simplifyGraphLine(dense,.2);
    expect(reduced.size()<dense.size()/2,"Display simplification did not reduce points");
    double maxError=0;
    for (auto point:dense) {
      double nearest=1e100;
      for(qsizetype j=1;j<reduced.size();++j) {
        auto d=reduced[j]-reduced[j-1],v=point-reduced[j-1];
        double n=QPointF::dotProduct(d,d),u=n>0?std::clamp(QPointF::dotProduct(v,d)/n,0.,1.):0.;
        auto error=v-u*d; nearest=std::min(nearest,std::sqrt(QPointF::dotProduct(error,error)));
      }
      maxError=std::max(maxError,nearest);
    }
    expect(maxError<=.200001 && reduced.contains(dense[503]),"Screen simplification lost a visible narrow peak");
    navigation.snapshot.metric=si::Metric::RL;
    navigation.snapshot.curves[0].start=1e9;navigation.snapshot.curves[0].stop=3e9;
    navigation.snapshot.curves[0].plotX={1e9,2e9,3e9};navigation.snapshot.curves[0].plotY={-20,-10,-15};
    navigation.resetView();double marker=-1;
    navigation.markerAdded=[&](double frequency){marker=frequency;};
    const auto center=graphArea(navigation.rect(),navigation.snapshot).center();
    QMouseEvent dbl(QEvent::MouseButtonDblClick,center,center,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&navigation,&dbl);
    expect(std::abs(marker-2e9)<1e-5,"Frequency double click retains exact marker insertion");
    navigation.snapshot.heatmap=true;
    navigation.snapshot.curves[0].channel="row-one";
    navigation.snapshot.curves[0].heatRaw={-15,-12};
    auto nextRow=navigation.snapshot.curves[0];nextRow.channel="row-two";nextRow.heatRaw={-35,-32};
    navigation.snapshot.curves.push_back(nextRow);
    navigation.resetView();
    QString cellText;navigation.cursorChanged=[&](const QString &text){cellText=text;};
    auto heatArea=graphArea(navigation.rect(),navigation.snapshot);
    QPointF cell(heatArea.left()+heatArea.width()*.75,heatArea.top()+heatArea.height()*.75);
    QMouseEvent cellMove(QEvent::MouseMove,cell,cell,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&navigation,&cellMove);
    expect(cellText.contains("row-two") && cellText.contains("-32.000") && cellText.contains("cell worst"),
           "Heatmap cursor reports the pointed row and bucket value");
    wheel(Qt::ControlModifier);
    expect(std::isnan(navigation.snapshot.viewBottom),"Heatmap wheel preserves categorical Y rows");

    std::cout << "PASS " << checks
              << " Qt checks; GUI heartbeats=" << heartbeats << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
