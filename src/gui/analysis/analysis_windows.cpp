#include "gui/analysis/analysis_windows.hpp"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QFileDialog>
#include <QTextStream>
#include <exception>
#include <stdexcept>
#include <utility>

namespace si::gui {
namespace {
QString labelFor(Feature feature) {
  switch (feature) {
    case Feature::Ild: return QStringLiteral("ILD");
    case Feature::Icn: return QStringLiteral("ICN");
    case Feature::Eye: return QStringLiteral("Eye Diagram");
    case Feature::Com: return QStringLiteral("COM");
  }
  return QStringLiteral("Unknown analysis");
}
QString statusName(si::channel::Status status) {
  using S = si::channel::Status;
  switch (status) {
    case S::Valid: return QStringLiteral("VALID");
    case S::Warning: return QStringLiteral("WARNING");
    case S::Stale: return QStringLiteral("STALE");
    case S::NotEvaluated: return QStringLiteral("NOT EVALUATED");
    case S::Unsupported: return QStringLiteral("UNSUPPORTED");
    case S::InvalidSource: return QStringLiteral("INVALID SOURCE");
    case S::Cancelled: return QStringLiteral("CANCELLED");
    case S::Error: return QStringLiteral("ERROR");
  }
  return QStringLiteral("ERROR");
}

class IldPlotWidget final : public QWidget {
public:
  explicit IldPlotWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName(QStringLiteral("ildResultGraph"));
    setMinimumHeight(260);
    setMouseTracking(true);
  }
  void setData(std::shared_ptr<const si::channel::IldData> data) {
    data_ = std::move(data);
    if(data_ && data_->frequencyHz.size()>1) {
      viewFrom_=data_->frequencyHz.front();
      viewTo_=data_->frequencyHz.back();
    } else {viewFrom_=0;viewTo_=0;}
    update();
  }
protected:
  void wheelEvent(QWheelEvent *e) override {
    if(!data_ || viewTo_<=viewFrom_ || !e->angleDelta().y()) return;
    const double full0=data_->frequencyHz.front(), full1=data_->frequencyHz.back();
    const double fullSpan=full1-full0, span=viewTo_-viewFrom_;
    const double newSpan=std::clamp(span*(e->angleDelta().y()>0?0.8:1.25),
      fullSpan/4096.0,fullSpan);
    const double portion=std::clamp((e->position().x()-70.0)/
                                   std::max(1.0,double(width()-92)),0.0,1.0);
    const double center=viewFrom_+portion*span;
    viewFrom_=std::clamp(center-portion*newSpan,full0,full1-newSpan);
    viewTo_=viewFrom_+newSpan;
    update();
    e->accept();
  }
  void mousePressEvent(QMouseEvent *e) override {
    if(data_ && e->button()==Qt::LeftButton) {
      dragging_=true;lastX_=e->position().x();e->accept();
    } else QWidget::mousePressEvent(e);
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if(!data_)return;
    if(dragging_ && viewTo_>viewFrom_) {
      const double dx=e->position().x()-lastX_;
      lastX_=e->position().x();
      const double span=viewTo_-viewFrom_;
      const double full0=data_->frequencyHz.front(),full1=data_->frequencyHz.back();
      viewFrom_=std::clamp(viewFrom_-dx*span/std::max(1.0,double(width()-92)),
                           full0,full1-span);
      viewTo_=viewFrom_+span;
      update();
    } else {
      const double t=std::clamp((e->position().x()-70.0)/
                                std::max(1.0,double(width()-92)),0.0,1.0);
      const double frequency=viewFrom_+t*(viewTo_-viewFrom_);
      const auto &xs=data_->frequencyHz;
      auto it=std::lower_bound(xs.begin(),xs.end(),frequency);
      const size_t i=it==xs.end()?xs.size()-1:size_t(it-xs.begin());
      setToolTip(QStringLiteral("%1 GHz | IL %2 dB | fit %3 dB | ILD %4 dB")
        .arg(xs[i]/1e9,0,'g',8).arg(data_->insertionLossDb[i],0,'g',7)
        .arg(data_->fittedLossDb[i],0,'g',7)
        .arg(data_->deviationDb[i],0,'g',7));
    }
    e->accept();
  }
  void mouseReleaseEvent(QMouseEvent *e) override {
    dragging_=false; QWidget::mouseReleaseEvent(e);
  }
  void mouseDoubleClickEvent(QMouseEvent *e) override {
    if(data_) {
      viewFrom_=data_->frequencyHz.front();
      viewTo_=data_->frequencyHz.back();
      update();e->accept();
    } else QWidget::mouseDoubleClickEvent(e);
  }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), palette().base());
    p.setRenderHint(QPainter::Antialiasing);
    if (!data_ || data_->frequencyHz.size() < 2) {
      p.setPen(palette().text().color());
      p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Run ILD to display measured/fitted IL and deviation."));
      return;
    }
    const auto &d = *data_;
    const int n = int(d.frequencyHz.size());
    const double f0 = viewFrom_, f1 = viewTo_;
    if (!(f1 > f0)) return;
    const int padX = 70, right = 22;
    for (int panel=0;panel<2;++panel) {
      QRectF area(padX, 24 + panel*(height()-35)/2,
                  std::max(50,width()-padX-right),
                  std::max(55,(height()-35)/2-35));
      const auto &y0 = panel==0 ? d.insertionLossDb : d.deviationDb;
      double low = 0, high = 0;
      for (double v : y0) { low=std::min(low,v);high=std::max(high,v); }
      if(panel==0) for(double v:d.fittedLossDb) {
        low=std::min(low,v);high=std::max(high,v);
      }
      const double padding = std::max(0.05,(high-low)*0.08);
      low -= padding; high += padding;
      p.setPen(QPen(palette().mid().color(),1));
      p.drawRect(area);
      p.setPen(palette().text().color());
      p.drawText(QRectF(4,area.top(),padX-10,20), Qt::AlignRight,
        QString::number(high,'g',4));
      p.drawText(QRectF(4,area.bottom()-20,padX-10,20),Qt::AlignRight,
        QString::number(low,'g',4));
      p.drawText(QRectF(area.left(),area.top()-20,area.width(),18),Qt::AlignLeft,
        panel==0?QStringLiteral("Insertion loss (dB) — raw / fitted")
                :QStringLiteral("ILD(f) (dB) — raw minus fitted"));
      p.drawText(QRectF(area.left(),area.bottom()+2,area.width(),18),Qt::AlignLeft,
        QString::number(f0/1e9,'g',5)+QStringLiteral(" GHz"));
      p.drawText(QRectF(area.left(),area.bottom()+2,area.width(),18),Qt::AlignRight,
        QString::number(f1/1e9,'g',5)+QStringLiteral(" GHz"));
      p.save(); p.setClipRect(area);
      auto drawSeries=[&](const std::vector<double> &data,QColor color) {
        const int stride=std::max(1,n/4000);
        QPainterPath path;
        bool first = true;
        auto vertex=[&](int i) {
          double x=area.left()+(d.frequencyHz[size_t(i)]-f0)/(f1-f0)*area.width();
          double y=area.bottom()-(data[size_t(i)]-low)/(high-low)*area.height();
          if(first){path.moveTo(x,y);first=false;} else path.lineTo(x,y);
        };
        // Every group emits its extrema, avoiding loss of narrow peaks.
        for(int i=0;i<n;i+=stride) {
          int end=std::min(n,i+stride),mini=i,maxi=i;
          for(int j=i+1;j<end;++j) {
            if(data[size_t(j)]<data[size_t(mini)])mini=j;
            if(data[size_t(j)]>data[size_t(maxi)])maxi=j;
          }
          if(mini<=maxi) { vertex(mini);if(mini!=maxi)vertex(maxi); }
          else {vertex(maxi);vertex(mini);}
        }
        if((n-1)%stride)vertex(n-1);
        p.setPen(QPen(color,1.7)); p.drawPath(path);
      };
      if(panel==0) {
        drawSeries(d.insertionLossDb,QColor("#176eae"));
        drawSeries(d.fittedLossDb,QColor("#e26c32"));
      } else drawSeries(d.deviationDb,QColor("#8b4cb8"));
      p.restore();
    }
  }
private:
  std::shared_ptr<const si::channel::IldData> data_;
  double viewFrom_=0,viewTo_=0,lastX_=0;
  bool dragging_=false;
};

class IcnPlotWidget final : public QWidget {
public:
  explicit IcnPlotWidget(QWidget *parent=nullptr) : QWidget(parent) {
    setObjectName(QStringLiteral("icnResultGraph"));
    setMinimumHeight(410);
  }
  void setData(std::shared_ptr<const si::channel::IcnData> data) {
    data_=std::move(data);
    update();
  }
protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(),palette().base());
    p.setRenderHint(QPainter::Antialiasing);
    if(!data_ || data_->frequencyHz.size()<2) {
      p.setPen(palette().text().color());
      p.drawText(rect(),Qt::AlignCenter,QStringLiteral(
        "Select NEXT/FEXT aggressors and run the generic ICN model."));
      return;
    }
    const auto &d=*data_;
    const double f0=d.frequencyHz.front(), f1=d.frequencyHz.back();
    const size_t n=d.frequencyHz.size();
    if(!(f1>f0)) return;
    for(int panel=0;panel<3;++panel) {
      const double heightPer=double(height())/3.0;
      QRectF area(80,25+heightPer*panel,std::max(60,width()-100),
                  std::max(28.0,heightPer-45));
      auto sample=[&](size_t i,int which)->double {
        if(panel==0) return which?d.fextPowerDb[i]:d.nextPowerDb[i];
        if(panel==1) return (which?d.fextDensityV2Hz[i]:d.nextDensityV2Hz[i])*1e15;
        return (which?d.fextCumulativeRmsV[i]:d.nextCumulativeRmsV[i])*1000.0;
      };
      double low=0,high=0;
      for(size_t i=0;i<n;++i)
        for(int w=0;w<2;++w) {
          low=std::min(low,sample(i,w));
          high=std::max(high,sample(i,w));
        }
      const double extra=std::max((high-low)*0.08,0.05);
      low-=extra;high+=extra;
      p.setPen(QPen(palette().mid().color(),1));
      p.drawRect(area);
      p.setPen(palette().text().color());
      const QString title=panel==0?QStringLiteral("NEXT / FEXT power sum (dB, -300 dB zero floor)")
        :panel==1?QStringLiteral("Weighted noise PSD (mV²/GHz)")
                 :QStringLiteral("Cumulative NEXT / FEXT RMS (mV)");
      p.drawText(QRectF(area.left(),area.top()-21,area.width(),18),Qt::AlignLeft,title);
      p.drawText(QRectF(2,area.top(),75,20),Qt::AlignRight,QString::number(high,'g',4));
      p.drawText(QRectF(2,area.bottom()-19,75,20),Qt::AlignRight,QString::number(low,'g',4));
      p.drawText(QRectF(area.left(),area.bottom()+3,area.width(),18),Qt::AlignLeft,
                 QString::number(f0/1e9,'g',5)+QStringLiteral(" GHz"));
      p.drawText(QRectF(area.left(),area.bottom()+3,area.width(),18),Qt::AlignRight,
                 QString::number(f1/1e9,'g',5)+QStringLiteral(" GHz"));
      p.save();
      p.setClipRect(area);
      for(int which=0;which<2;++which) {
        const size_t stride=std::max<size_t>(1,n/3200);
        QPainterPath path;
        bool first=true;
        auto vertex=[&](size_t i) {
          const double x=area.left()+(d.frequencyHz[i]-f0)/(f1-f0)*area.width();
          const double y=area.bottom()-(sample(i,which)-low)/(high-low)*area.height();
          if(first) {path.moveTo(x,y);first=false;} else path.lineTo(x,y);
        };
        for(size_t i=0;i<n;i+=stride) {
          const size_t end=std::min(n,i+stride);
          size_t mini=i,maxi=i;
          for(size_t k=i+1;k<end;++k) {
            if(sample(k,which)<sample(mini,which))mini=k;
            if(sample(k,which)>sample(maxi,which))maxi=k;
          }
          if(mini<=maxi) {vertex(mini);if(mini!=maxi)vertex(maxi);}
          else {vertex(maxi);vertex(mini);}
        }
        vertex(n-1);
        p.setPen(QPen(which?QColor("#d47a24"):QColor("#236db6"),1.6));
        p.drawPath(path);
      }
      p.restore();
    }
  }
private:
  std::shared_ptr<const si::channel::IcnData> data_;
};
}
struct AsyncScheduler::Ticket {
  Feature feature;
  quint64 generation;
  QPointer<QObject> recipient;
  std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
  std::shared_ptr<std::atomic_int> progress = std::make_shared<std::atomic_int>(0);
  QFutureWatcher<WorkOutcome> *watcher = nullptr;
  WorkCompletion completion;
};

AsyncScheduler::AsyncScheduler(QObject *parent) : QObject(parent) {
  pool_.setMaxThreadCount(4);
  pool_.setExpiryTimeout(30000);
}
AsyncScheduler::~AsyncScheduler() {
  invalidate();
  // Workers capture only value data/atomics, never QObject pointers. Shutdown
  // cooperatively; an engine must check cancellation at bounded intervals.
  pool_.waitForDone();
}
quint64 AsyncScheduler::submit(Feature feature, QObject *recipient,
                              AsyncWork work, WorkCompletion completion) {
  if (!recipient || !work || !completion)
    throw std::invalid_argument("Analysis job requires a live recipient, work and completion");
  if (running(feature) || tickets_.size() >= featureCount)
    throw std::runtime_error("Analysis worker capacity exceeded");
  const quint64 id = ++serial_;
  auto ticket = std::make_shared<Ticket>();
  ticket->feature = feature;
  ticket->generation = generation_;
  ticket->recipient = recipient;
  ticket->completion = std::move(completion);
  auto *watcher = new QFutureWatcher<WorkOutcome>(this);
  ticket->watcher = watcher;
  tickets_.insert(id, ticket);
  connect(watcher, &QFutureWatcher<WorkOutcome>::finished, this, [this, id] {
    auto ticket = tickets_.take(id);
    if (!ticket) return;
    const bool stale = ticket->generation != generation_;
    WorkOutcome outcome;
    try {
      outcome = ticket->watcher->result();
    } catch (const std::exception &e) {
      outcome = {si::channel::Status::Error, QString::fromUtf8(e.what())};
    } catch (...) {
      outcome = {si::channel::Status::Error, QStringLiteral("Unknown worker failure")};
    }
    ticket->watcher->deleteLater();
    // No callback into a destroyed window. A cancelled/stale calculation must
    // never be published as a valid result.
    if (ticket->recipient) ticket->completion(outcome, stale);
  });
  try {
    auto future = QtConcurrent::run(&pool_,
        [work = std::move(work), cancelled = ticket->cancelled,
         progress = ticket->progress]() -> WorkOutcome {
          try {
            if (cancelled->load(std::memory_order_acquire))
              return {si::channel::Status::Cancelled, QStringLiteral("Cancelled")};
            auto outcome = work(*cancelled, *progress);
            if (cancelled->load(std::memory_order_acquire))
              return {si::channel::Status::Cancelled, QStringLiteral("Cancelled")};
            return outcome;
          } catch (const si::Cancelled &) {
            return {si::channel::Status::Cancelled, QStringLiteral("Calculation cancelled")};
          } catch (const std::exception &e) {
            if(cancelled->load(std::memory_order_acquire))
              return {si::channel::Status::Cancelled, QStringLiteral("Calculation cancelled")};
            return {si::channel::Status::Error, QString::fromUtf8(e.what())};
          } catch (...) {
            return {si::channel::Status::Error, QStringLiteral("Unknown worker failure")};
          }
        });
    watcher->setFuture(future);
  } catch (...) {
    tickets_.remove(id);
    delete watcher;
    throw;
  }
  return id;
}
void AsyncScheduler::cancel(Feature feature) {
  for (auto it = tickets_.begin(); it != tickets_.end(); ++it)
    if (it.value()->feature == feature)
      it.value()->cancelled->store(true, std::memory_order_release);
}
void AsyncScheduler::invalidate() {
  ++generation_;
  for (auto it = tickets_.begin(); it != tickets_.end(); ++it)
    it.value()->cancelled->store(true, std::memory_order_release);
}
bool AsyncScheduler::running(Feature feature) const {
  for (auto it = tickets_.cbegin(); it != tickets_.cend(); ++it)
    if (it.value()->feature == feature) return true;
  return false;
}
int AsyncScheduler::progress(Feature feature) const {
  for (auto it = tickets_.cbegin(); it != tickets_.cend(); ++it)
    if (it.value()->feature == feature)
      return std::clamp(it.value()->progress->load(std::memory_order_relaxed), 0, 100);
  return 0;
}
int AsyncScheduler::inFlight() const { return int(tickets_.size()); }

AnalysisWindow::AnalysisWindow(Feature feature, AsyncScheduler *scheduler,
                               QWidget *owner)
    : QWidget(owner, Qt::Window), feature_(feature), scheduler_(scheduler) {
  setAttribute(Qt::WA_DeleteOnClose);
  setObjectName(QStringLiteral("channelAnalysisWindow") + labelFor(feature));
  setWindowTitle(QStringLiteral("SParamView · ") + labelFor(feature));
  resize(940, 640);
  auto *outer = new QVBoxLayout(this);
  auto *header = new QLabel(QStringLiteral("<b>") + labelFor(feature_) +
       QStringLiteral("</b> — Channel Analysis"));
  header->setTextFormat(Qt::RichText);
  outer->addWidget(header);
  auto *form = new QFormLayout;
  source_ = new QComboBox;
  source_->setObjectName(QStringLiteral("analysisSource"));
  victim_ = new QComboBox;
  victim_->setObjectName(QStringLiteral("analysisVictim"));
  aggressor_ = new QComboBox;
  aggressor_->setObjectName(QStringLiteral("analysisAggressor"));
  direction_ = new QComboBox;
  direction_->addItems({QStringLiteral("Forward"), QStringLiteral("Reverse")});
  direction_->setObjectName(QStringLiteral("analysisDirection"));
  fromGhz_ = new QDoubleSpinBox;
  toGhz_ = new QDoubleSpinBox;
  for (auto *spin : {fromGhz_, toGhz_}) {
    spin->setRange(0, 100000);
    spin->setDecimals(6);
    spin->setSuffix(QStringLiteral(" GHz"));
  }
  toGhz_->setValue(30);
  if (feature_ == Feature::Ild) {
    fitModel_ = new QComboBox;
    fitModel_->setObjectName(QStringLiteral("ildFitModel"));
    fitModel_->addItems({QStringLiteral("Linear"), QStringLiteral("Root + linear"),
                         QStringLiteral("Root + linear + quadratic")});
    fitModel_->setCurrentIndex(2);
    form->addRow(QStringLiteral("ILD fit model"),fitModel_);
  }
  form->addRow(QStringLiteral("Source / Revision"), source_);
  form->addRow(QStringLiteral("Victim Channel"), victim_);
  if (feature_ == Feature::Icn || feature_ == Feature::Com)
    form->addRow(QStringLiteral("Aggressor"), aggressor_);
  else aggressor_->hide();
  form->addRow(QStringLiteral("Direction"), direction_);
  form->addRow(QStringLiteral("Start frequency"), fromGhz_);
  form->addRow(QStringLiteral("Stop frequency"), toGhz_);
  outer->addLayout(form);
  auto *controls = new QHBoxLayout;
  run_ = new QPushButton(QStringLiteral("Run"));
  run_->setObjectName(QStringLiteral("analysisRun"));
  run_->setToolTip(QStringLiteral("Numerical engine pending in a later 012 stage"));
  run_->setEnabled(false);
  cancel_ = new QPushButton(QStringLiteral("Cancel"));
  cancel_->setObjectName(QStringLiteral("analysisCancel"));
  cancel_->setEnabled(false);
  controls->addWidget(run_);
  controls->addWidget(cancel_);
  progress_ = new QProgressBar;
  progress_->setRange(0, 100);
  progress_->setValue(0);
  progress_->setObjectName(QStringLiteral("analysisProgress"));
  controls->addWidget(progress_, 1);
  outer->addLayout(controls);
  statusLabel_ = new QLabel;
  statusLabel_->setObjectName(QStringLiteral("analysisStatus"));
  statusLabel_->setWordWrap(true);
  outer->addWidget(statusLabel_);
  graphPlaceholder_ = new QLabel(QStringLiteral("No numerical data. Analysis engine is not installed."));
  graphPlaceholder_->setObjectName(QStringLiteral("analysisGraphPlaceholder"));
  graphPlaceholder_->setAlignment(Qt::AlignCenter);
  graphPlaceholder_->setMinimumHeight(180);
  graphPlaceholder_->setFrameStyle(QFrame::StyledPanel);
  outer->addWidget(graphPlaceholder_, 1);
  if(feature_ == Feature::Ild) {
    graphPlaceholder_->hide();
    ildGraph_=new IldPlotWidget(this);
    outer->addWidget(ildGraph_,2);
  }
  auto *exports = new QHBoxLayout;
  csv_ = new QPushButton(QStringLiteral("Export CSV"));
  png_ = new QPushButton(QStringLiteral("Export PNG"));
  csv_->setObjectName(QStringLiteral("analysisCsv"));
  png_->setObjectName(QStringLiteral("analysisPng"));
  csv_->setEnabled(false);
  png_->setEnabled(false);
  exports->addWidget(csv_);
  exports->addWidget(png_);
  exports->addStretch();
  outer->addLayout(exports);
  setStatus(si::channel::Status::NotEvaluated,
            QStringLiteral("Engine not yet implemented; no simulated or conformant results"));
  connect(cancel_, &QPushButton::clicked, this, [this] { cancelWork(); });
  if(feature_ == Feature::Ild) {
    connect(run_, &QPushButton::clicked,this,[this]{launchIld();});
    connect(csv_, &QPushButton::clicked,this,[this]{exportIldCsv();});
    connect(png_, &QPushButton::clicked,this,[this]{exportIldPng();});
    connect(fitModel_, &QComboBox::currentIndexChanged,this,[this]{invalidateRequest();});
  }
  connect(source_, &QComboBox::currentIndexChanged, this, [this] {
    QSignalBlocker block(victim_);
    QSignalBlocker aggressiveBlock(aggressor_);
    victim_->clear();
    aggressor_->clear();
    const int index = source_->currentIndex();
    if (index >= 0 && index < context_.sources.size()) {
      const auto &selected = context_.sources[index];
      victim_->addItems(selected.channels);
      aggressor_->addItem(QStringLiteral("Select aggressor..."));
      aggressor_->addItems(selected.channels);
    }
    invalidateRequest();
  });
  for (auto *combo : {victim_, aggressor_, direction_})
    connect(combo, &QComboBox::currentIndexChanged, this, [this] { invalidateRequest(); });
  for (auto *spin : {fromGhz_, toGhz_})
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { invalidateRequest(); });
  progressPoll_.setInterval(40);
  connect(&progressPoll_, &QTimer::timeout, this, [this] {
    if (scheduler_ && scheduler_->running(feature_))
      progress_->setValue(scheduler_->progress(feature_));
    else progressPoll_.stop();
  });
}
void AnalysisWindow::setStatus(si::channel::Status status, const QString &details) {
  status_ = status;
  statusLabel_->setText(statusName(status) + QStringLiteral(" — ") + details);
  statusLabel_->setToolTip(statusLabel_->text());
}
QString AnalysisWindow::selectionIdentity() const {
  const int index = source_->currentIndex();
  if (index < 0 || index >= context_.sources.size()) return {};
  const auto &item = context_.sources[index];
  return item.sha256 + QLatin1Char('|') + item.revision + QLatin1Char('|') +
    item.mappingDigest + QLatin1Char('|') + victim_->currentText() +
    QLatin1Char('|') + aggressor_->currentText() + QLatin1Char('|') +
    QString::number(direction_->currentIndex()) + QLatin1Char('|') +
    QString::number(fromGhz_->value(), 'g', 17) + QLatin1Char('|') +
    QString::number(toGhz_->value(), 'g', 17) + QLatin1Char('|') +
    QString::number(fitModel_ ? fitModel_->currentIndex() : -1);
}
void AnalysisWindow::invalidateRequest() {
  clearIld();
  const bool hadWork = scheduler_ && scheduler_->running(feature_);
  ++requestGeneration_;
  if (scheduler_) scheduler_->cancel(feature_);
  cancel_->setEnabled(false);
  progress_->setValue(0);
  setStatus(hadWork || status_ == si::channel::Status::Valid ||
                    status_ == si::channel::Status::Warning
        ? si::channel::Status::Stale : si::channel::Status::NotEvaluated,
            QStringLiteral("Inputs changed; previous analysis is invalid"));
}
void AnalysisWindow::setContext(const AnalysisContext &context) {
  clearIld();
  const bool hadWork = scheduler_ && scheduler_->running(feature_);
  const bool hadResult = status_ == si::channel::Status::Valid ||
                         status_ == si::channel::Status::Warning;
  if (scheduler_) scheduler_->cancel(feature_);
  context_ = context;
  ++requestGeneration_;
  progress_->setValue(0);
  cancel_->setEnabled(false);
  QSignalBlocker sourceBlock(source_);
  QSignalBlocker victimBlock(victim_);
  QSignalBlocker aggressorBlock(aggressor_);
  source_->clear();
  victim_->clear();
  aggressor_->clear();
  for (const auto &item : context_.sources)
    source_->addItem(item.revision);
  if (context_.activeSource >= 0 && context_.activeSource < context_.sources.size()) {
    source_->setCurrentIndex(context_.activeSource);
    const auto &item = context_.sources[context_.activeSource];
    victim_->addItems(item.channels);
    aggressor_->addItem(QStringLiteral("Select aggressor..."));
    aggressor_->addItems(item.channels);
  } else source_->setCurrentIndex(-1);
  setStatus(hadWork || hadResult ? si::channel::Status::Stale
           : si::channel::Status::NotEvaluated,
           feature_ == Feature::Ild && ildProvider_ ?
             QStringLiteral("Context updated; run ILD on the confirmed mapping") :
             QStringLiteral("Context updated; analysis engine not installed"));
}
bool AnalysisWindow::startWork(AsyncWork work) {
  const int index = source_->currentIndex();
  if (!scheduler_ || !work || index < 0 || index >= context_.sources.size())
    return false;
  const auto &item = context_.sources[index];
  if (!item.mappingConfirmed || item.sha256.isEmpty() || item.mappingDigest.isEmpty() ||
      victim_->currentText().isEmpty() || fromGhz_->value() >= toGhz_->value()) {
    setStatus(si::channel::Status::InvalidSource,
              QStringLiteral("Confirmed source, mapping and frequency band required"));
    return false;
  }
  if (scheduler_->running(feature_)) return false;
  const quint64 requestGeneration = ++requestGeneration_;
  const quint64 contextGeneration = context_.generation;
  const QString selected = selectionIdentity();
  QPointer<AnalysisWindow> guard(this);
  try {
    scheduler_->submit(feature_, this, std::move(work),
      [guard, selected, requestGeneration, contextGeneration]
      (const WorkOutcome &outcome, bool stale) {
        if (!guard) return;
        guard->progressPoll_.stop();
        guard->cancel_->setEnabled(false);
        if (stale || requestGeneration != guard->requestGeneration_ ||
            contextGeneration != guard->context_.generation ||
            selected != guard->selectionIdentity()) {
          guard->setStatus(si::channel::Status::Stale,
              QStringLiteral("Older result discarded after context/settings change"));
          return;
        }
        guard->progress_->setValue(outcome.status == si::channel::Status::Valid ? 100 : 0);
        guard->setStatus(outcome.status, outcome.diagnostic);
        if (outcome.ild && (outcome.status == si::channel::Status::Valid ||
                            outcome.status == si::channel::Status::Warning))
          guard->showIld(outcome);
      });
  } catch (const std::exception &e) {
    setStatus(si::channel::Status::Error, QString::fromUtf8(e.what()));
    return false;
  }
  clearIld();
  cancel_->setEnabled(true);
  setStatus(si::channel::Status::NotEvaluated, QStringLiteral("Calculation running"));
  progressPoll_.start();
  return true;
}
void AnalysisWindow::cancelWork() {
  if (scheduler_) scheduler_->cancel(feature_);
  cancel_->setEnabled(false);
  setStatus(si::channel::Status::Cancelled,
            QStringLiteral("Cancellation requested; pending work cannot publish results"));
}
void AnalysisWindow::clearIld() {
  ildResult_.reset();
  if(ildGraph_) static_cast<IldPlotWidget *>(ildGraph_)->setData({});
  if(csv_) csv_->setEnabled(false);
  if(png_) png_->setEnabled(false);
}
void AnalysisWindow::setIldSourceProvider(IldSourceProvider provider) {
  ildProvider_=std::move(provider);
  if(feature_ == Feature::Ild) {
    run_->setEnabled(bool(ildProvider_));
    run_->setToolTip(ildProvider_?QStringLiteral("Run verified ILD fit")
                :QStringLiteral("ILD source provider unavailable"));
  }
}
void AnalysisWindow::launchIld() {
  if(feature_ != Feature::Ild || !ildProvider_) return;
  const int idx=source_->currentIndex();
  if(idx<0 || idx>=context_.sources.size() ||
      victim_->currentIndex()<0 || fromGhz_->value()>=toGhz_->value()) {
    setStatus(si::channel::Status::InvalidSource,QStringLiteral("Choose confirmed source, channel and frequency range"));
    return;
  }
  const auto &source=context_.sources[idx];
  const int victimIndex=victim_->currentIndex();
  if(!source.mappingConfirmed || victimIndex>=source.channelIds.size()) {
    setStatus(si::channel::Status::InvalidSource,QStringLiteral("Missing confirmed physical channel identity"));
    return;
  }
  const auto input=ildProvider_(idx);
  if(!input || !input->cache || input->expectedSha != source.sha256.toStdString() ||
      input->revision != source.revision.toStdString()) {
    setStatus(si::channel::Status::InvalidSource,QStringLiteral("Source revision changed; refresh analysis context"));
    return;
  }
  const std::string victimId=source.channelIds[victimIndex].toStdString();
  const bool reverse=direction_->currentIndex()==1;
  si::channel::IldSettings cfg;
  cfg.startHz=fromGhz_->value()*1e9;
  cfg.stopHz=toGhz_->value()*1e9;
  cfg.model=static_cast<si::channel::IldFitModel>(fitModel_->currentIndex());
  const std::string fitIdentity=selectionIdentity().toStdString();
  const bool accepted=startWork([input=*input,victimId,reverse,cfg,fitIdentity]
     (const std::atomic_bool &cancel,std::atomic_int &progress)->WorkOutcome {
    if(cancel.load()) return {si::channel::Status::Cancelled,QStringLiteral("Cancelled")};
    progress.store(10);
    // Capture/re-hash on worker thread; never use live GUI objects.
    auto snapshot=si::channel::SourceSnapshot::capture(
      input.cache,input.mapping,input.revision,input.workspaceGeneration,
      input.mappingGeneration);
    if(cancel.load()) return {si::channel::Status::Cancelled,QStringLiteral("Cancelled")};
    if(snapshot.identity().sha256 != input.expectedSha)
      throw si::Error("Revision SHA mismatch");
    auto trace=si::channel::extract(snapshot,victimId,si::Metric::IL,reverse);
    if(cancel.load()) return {si::channel::Status::Cancelled,QStringLiteral("Cancelled")};
    progress.store(65);
    const auto &identity=snapshot.identity();
    si::channel::Provenance provenance{identity.sha256,identity.revision,
      identity.mappingIdentity,"Generic ILD least-squares","012_05-v1",
      fitIdentity,identity.workspaceGeneration,identity.mappingGeneration};
    auto output=si::channel::calculateIld(trace,cfg,provenance,&cancel);
    if(cancel.load()) return {si::channel::Status::Cancelled,QStringLiteral("Cancelled")};
    auto data=std::make_shared<si::channel::IldData>(std::move(*output.value));
    progress.store(100);
    WorkOutcome outcome;
    outcome.status=output.status;
    outcome.diagnostic=QStringLiteral("Exploratory ILD; no IEEE/MIPI PASS or FAIL");
    outcome.ild=std::move(data);
    outcome.provenance=std::move(output.provenance);
    return outcome;
  });
  if(!accepted) return;
}
void AnalysisWindow::showIld(const WorkOutcome &outcome) {
  if(!outcome.ild) return;
  ildResult_=outcome.ild;
  ildProvenance_=outcome.provenance;
  static_cast<IldPlotWidget *>(ildGraph_)->setData(ildResult_);
  csv_->setEnabled(true);
  png_->setEnabled(true);
  const auto &d=*ildResult_;
  setStatus(outcome.status,QStringLiteral("RMS %1 dB | peak abs %2 dB @ %3 GHz | %4 samples. Exploratory: no standard PASS")
    .arg(d.rmsDb,0,'g',6).arg(d.peakAbsoluteDb,0,'g',6)
    .arg(d.peakFrequencyHz/1e9,0,'g',7).arg(qulonglong(d.frequencyHz.size())));
}
void AnalysisWindow::exportIldCsv() {
  if(!ildResult_) return;
  const QString name=QFileDialog::getSaveFileName(this,QStringLiteral("Export ILD data"),
                          QStringLiteral("ILD.csv"),QStringLiteral("CSV (*.csv)"));
  if(!name.isEmpty()) saveIldCsv(name);
}
bool AnalysisWindow::saveIldCsv(const QString &name) {
  if(!ildResult_ || name.isEmpty()) return false;
  QSaveFile file(name);
  if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
    setStatus(si::channel::Status::Error,QStringLiteral("Cannot open CSV output"));return false;
  }
  QTextStream out(&file);
  const auto &d=*ildResult_;
  auto safe=[](const std::string &raw) {
    QString v=QString::fromStdString(raw);
    v.replace(QLatin1Char('\r'),QLatin1Char(' '));
    v.replace(QLatin1Char('\n'),QLatin1Char(' '));
    v.replace(QStringLiteral("\""),QStringLiteral("\"\""));
    const QString trimmed=v.trimmed();
    if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))v.prepend(QLatin1Char('\''));
    return QStringLiteral("\"")+v+QStringLiteral("\"");
  };
  out<<"# source_sha256,"<<safe(ildProvenance_.sourceSha256)<<"\n"
     <<"# revision,"<<safe(ildProvenance_.revision)<<"\n"
     <<"# mapping,"<<safe(ildProvenance_.mappingIdentity)<<"\n"
     <<"# settings_identity,"<<safe(ildProvenance_.settingsIdentity)<<"\n"
     <<"# parameter,"<<safe(d.parameter)<<"\n"
     <<"# algorithm,"<<safe(ildProvenance_.algorithm)<<"\n"
     <<"# algorithm_version,"<<safe(ildProvenance_.version)<<"\n"
     <<"# fitting_model,"<<safe(d.modelName)<<"\n"
     <<"# frequency_normalization_hz,"<<QString::number(d.frequencyScaleHz,'g',17)<<"\n"
     <<"# fit_start_hz,"<<QString::number(d.rangeStartHz,'g',17)<<"\n"
     <<"# fit_stop_hz,"<<QString::number(d.rangeStopHz,'g',17)<<"\n"
     <<"# rms_db,"<<QString::number(d.rmsDb,'g',17)<<"\n"
     <<"# peak_abs_db,"<<QString::number(d.peakAbsoluteDb,'g',17)<<"\n";
  for(size_t i=0;i<d.coefficients.size();++i)
    out<<"# a"<<i<<","<<QString::number(d.coefficients[i],'g',17)<<"\n";
  out<<"Frequency_GHz,IL_dB,Fitted_IL_dB,ILD_dB\n";
  for(size_t i=0;i<d.frequencyHz.size();++i)
    out<<QString::number(d.frequencyHz[i]/1e9,'g',17)<<","
       <<QString::number(d.insertionLossDb[i],'g',17)<<","
       <<QString::number(d.fittedLossDb[i],'g',17)<<","
       <<QString::number(d.deviationDb[i],'g',17)<<"\n";
  out.flush();
  const bool okay = out.status()==QTextStream::Ok && file.commit();
  if(!okay) setStatus(si::channel::Status::Error,QStringLiteral("ILD CSV write failed"));
  return okay;
}
void AnalysisWindow::exportIldPng() {
  if(!ildResult_ || !ildGraph_) return;
  const QString name=QFileDialog::getSaveFileName(this,QStringLiteral("Export ILD plot"),
                       QStringLiteral("ILD.png"),QStringLiteral("PNG (*.png)"));
  if(!name.isEmpty()) saveIldPng(name);
}
bool AnalysisWindow::saveIldPng(const QString &name) {
  if(!ildResult_ || !ildGraph_ || name.isEmpty()) return false;
  QSaveFile file(name);
  const bool okay=file.open(QIODevice::WriteOnly) &&
                   ildGraph_->grab().save(&file,"PNG") && file.commit();
  if(!okay) setStatus(si::channel::Status::Error,QStringLiteral("ILD PNG write failed"));
  return okay;
}
void AnalysisWindow::closeEvent(QCloseEvent *event) {
  if (scheduler_ && scheduler_->running(feature_)) cancelWork();
  progressPoll_.stop();
  QWidget::closeEvent(event);
}

AnalysisWindowManager::AnalysisWindowManager(QMainWindow *owner)
    : QObject(owner), owner_(owner), scheduler_(this) {
  retirementPoll_.setInterval(50);
  connect(&retirementPoll_, &QTimer::timeout, this, [this] {
    if (scheduler_.inFlight() != 0) return;
    retirementPoll_.stop();
    for (auto &workspace : retiredWorkspaces_)
      if (workspace && !workspace->remove())
        qWarning() << "SParamView: retired analysis workspace cleanup deferred";
    retiredWorkspaces_.clear();
  });
}
AnalysisWindowManager::~AnalysisWindowManager() {
  scheduler_.invalidate();
  retirementPoll_.stop();
  for (auto &window : windows_)
    if (window) window->close();
  // The scheduler member is destroyed (and its workers joined) before the
  // retained cache vector is destroyed; this ordering is intentional.
}
void AnalysisWindowManager::installMenu() {
  auto *menu = owner_->menuBar()->addMenu(QStringLiteral("Analysis"));
  menu->setObjectName(QStringLiteral("channelAnalysisMenu"));
  for (int i = 0; i < featureCount; ++i) {
    const auto feature = static_cast<Feature>(i);
    auto *action = menu->addAction(labelFor(feature));
    const QStringList names = {QStringLiteral("ILD"), QStringLiteral("ICN"),
                               QStringLiteral("Eye"), QStringLiteral("COM")};
    action->setObjectName(QStringLiteral("analysisAction") + names[i]);
    action->setEnabled(false);
    action->setToolTip(QStringLiteral("Available after the numerical engine is verified"));
    connect(action, &QAction::triggered, this, [this, feature] {
      if (available_[int(feature)]) openWindow(feature);
    });
    actions_[i] = action;
  }
}
void AnalysisWindowManager::setContext(AnalysisContext context) {
  context.generation = ++contextGeneration_;
  context_ = std::move(context);
  scheduler_.invalidate();
  for (auto &window : windows_)
    if (window) window->setContext(context_);
}
void AnalysisWindowManager::retireWorkspace(std::shared_ptr<QTemporaryDir> workspace) {
  if (!workspace) return;
  if (scheduler_.inFlight() == 0) {
    if (!workspace->remove())
      qWarning() << "SParamView: retired workspace cleanup deferred";
    return;
  }
  retiredWorkspaces_.push_back(std::move(workspace));
  retirementPoll_.start();
}
void AnalysisWindowManager::setIldSourceProvider(IldSourceProvider provider) {
  ildProvider_=std::move(provider);
  if(windows_[int(Feature::Ild)])
    windows_[int(Feature::Ild)]->setIldSourceProvider(ildProvider_);
  setEngineAvailable(Feature::Ild,bool(ildProvider_));
}
void AnalysisWindowManager::setEngineAvailable(Feature feature, bool available) {
  available_[int(feature)] = available;
  if (auto *action = actions_[int(feature)])
    action->setEnabled(available);
}
AnalysisWindow *AnalysisWindowManager::openWindow(Feature feature) {
  const int index = int(feature);
  if (windows_[index]) {
    windows_[index]->show();
    windows_[index]->raise();
    windows_[index]->activateWindow();
    return windows_[index];
  }
  auto *window = new AnalysisWindow(feature, &scheduler_, owner_);
  if(feature == Feature::Ild) window->setIldSourceProvider(ildProvider_);
  window->setContext(context_);
  windows_[index] = window;
  connect(window, &QObject::destroyed, this, [this, index] {
    windows_[index] = nullptr;
  });
  window->show();
  window->raise();
  window->activateWindow();
  return window;
}
} // namespace si::gui
