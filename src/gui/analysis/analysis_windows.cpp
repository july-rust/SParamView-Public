#include "../../user_preferences.hpp"
#include "gui/analysis/analysis_windows.hpp"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QStatusBar>
#include <QFileDialog>
#include <QTextStream>
#include <exception>
#include <stdexcept>
#include <utility>

namespace si::gui {
namespace {
// 012_43: COM remains a preserved research engine, not a production feature.
constexpr bool isReleaseFeature(Feature feature) noexcept {
  return feature == Feature::Ild || feature == Feature::Icn || feature == Feature::Eye;
}
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
      const int seriesCount=panel==0 ? 2 : 2+int(d.aggressors.size());
      auto sample=[&](size_t i,int which)->double {
        if(which>=2) {
          const auto &a=d.aggressors[size_t(which-2)];
          return panel==1?a.spectralDensityV2Hz[i]*1e15:a.cumulativeRmsV[i]*1000.0;
        }
        if(panel==0) return which?d.fextPowerDb[i]:d.nextPowerDb[i];
        if(panel==1) return (which?d.fextDensityV2Hz[i]:d.nextDensityV2Hz[i])*1e15;
        return (which?d.fextCumulativeRmsV[i]:d.nextCumulativeRmsV[i])*1000.0;
      };
      double low=0,high=0;
      for(size_t i=0;i<n;++i)
        for(int w=0;w<seriesCount;++w) {
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
      for(int which=0;which<seriesCount;++which) {
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
        const QColor color=which==0?QColor("#236db6"):
          which==1?QColor("#d47a24"):
          QColor::fromHsv((int(which)-2)*53%360,180,180);
        p.setPen(QPen(color,which<2?1.8:1.15));
        p.drawPath(path);
      }
      p.restore();
    }
  }
private:
  std::shared_ptr<const si::channel::IcnData> data_;
};

class EyePlotWidget final : public QWidget {
public:
  explicit EyePlotWidget(QWidget *parent=nullptr) : QWidget(parent) {
    setObjectName(QStringLiteral("eyeResultGraph"));
    setMinimumHeight(330);
    setMouseTracking(true);
  }
  void setData(std::shared_ptr<const si::channel::EyeData> data, double thresholdV) {
    data_=std::move(data); thresholdV_=thresholdV;
    zoom_=1; shift_=0; cursor_=-1; update();
  }
  void setView(int view) { view_=std::clamp(view,0,2); zoom_=1;shift_=0;update(); }
protected:
  void wheelEvent(QWheelEvent *e) override {
    if(!data_ || !e->angleDelta().y()) { QWidget::wheelEvent(e);return; }
    const double before=zoom_;
    zoom_=std::clamp(zoom_*(e->angleDelta().y()>0?1.25:0.8),1.0,32.0);
    const double portion=std::clamp((e->position().x()-75)/std::max(1.,double(width()-100)),0.,1.);
    shift_=std::clamp(shift_+portion*(1/before-1/zoom_),0.,1.-1/zoom_);
    update();e->accept();
  }
  void mousePressEvent(QMouseEvent *e) override {
    if(e->button()==Qt::LeftButton) { dragging_=true; lastX_=e->position().x();e->accept(); }
    else QWidget::mousePressEvent(e);
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if(dragging_) {
      shift_=std::clamp(shift_-(e->position().x()-lastX_)/
          std::max(1.,double(width()-100))/zoom_,0.,1.-1/zoom_);
      lastX_=e->position().x();
    }
    cursor_=std::clamp((e->position().x()-75)/std::max(1.,double(width()-100)),0.,1.);
    if(data_) {
      const double xFraction=shift_+cursor_/zoom_;
      const double xValue=view_==0?2*xFraction
                         :data_->timeSeconds.empty()?0:
                           xFraction*data_->timeSeconds.back()*1e12;
      setToolTip(QStringLiteral("Cursor: %1 %2; simulated only")
        .arg(xValue,0,'g',8).arg(view_==0?QStringLiteral("UI"):QStringLiteral("ps")));
    }
    lastX_=e->position().x();update();e->accept();
  }
  void mouseReleaseEvent(QMouseEvent *e) override {
    dragging_=false; QWidget::mouseReleaseEvent(e);
  }
  void mouseDoubleClickEvent(QMouseEvent *e) override {
    zoom_=1;shift_=0;update();e->accept();
  }
  void leaveEvent(QEvent *e) override { cursor_=-1;update();QWidget::leaveEvent(e); }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(),palette().base());
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(palette().text().color());
    const QString title=QStringLiteral("Simulated Channel Eye — Not Measured / Not PHY Compliance");
    p.drawText(QRectF(8,3,width()-16,24),Qt::AlignCenter,title);
    if(!data_) {
      p.drawText(rect(),Qt::AlignCenter,QStringLiteral("Enter verified electrical conditions and Run Eye"));
      return;
    }
    const auto &d=*data_;
    QRectF area(75,37,std::max(50,width()-100),std::max(50,height()-89));
    double low=std::numeric_limits<double>::infinity();
    double high=-std::numeric_limits<double>::infinity();
    auto include=[&](double v) {if(std::isfinite(v)){low=std::min(low,v);high=std::max(high,v);}};
    if(view_==0) {
      for(const auto &t:d.overlayV) for(double v:t) include(v*1000);
      include(thresholdV_*1000); include(d.minimumHighV*1000);include(d.maximumLowV*1000);
    } else if(view_==1) {
      for(double v:d.transmittedV)include(v*1000);
      for(double v:d.receivedV)include(v*1000);
    } else {
      for(double v:d.impulse.values) include(v);
      for(double v:d.unitPulse.values)include(v);
    }
    if(!std::isfinite(low)||!std::isfinite(high))return;
    const double margin=std::max((high-low)*0.10,0.001);
    low-=margin;high+=margin;
    p.setPen(palette().mid().color());p.drawRect(area);
    p.setPen(palette().text().color());
    p.drawText(QRectF(2,area.top(),69,20),Qt::AlignRight,QString::number(high,'g',6));
    p.drawText(QRectF(2,area.bottom()-20,69,20),Qt::AlignRight,QString::number(low,'g',6));
    p.drawText(QRectF(4,22,70,16),Qt::AlignRight,
      view_==2?QStringLiteral("relative"):QStringLiteral("mV"));
    const double fullX=view_==0?2.0 : (d.timeSeconds.empty()?0:d.timeSeconds.back()*1e12);
    const double x0=fullX*shift_, x1=fullX*(shift_+1/zoom_);
    if(x1<=x0) return;
    auto xp=[&](double x){return area.left()+(x-x0)/(x1-x0)*area.width();};
    auto yp=[&](double y){return area.bottom()-(y-low)/(high-low)*area.height();};
    p.drawText(QRectF(area.left(),area.bottom()+5,area.width(),20),Qt::AlignLeft,
      QString::number(x0,'g',7)+(view_==0?QStringLiteral(" UI"):QStringLiteral(" ps")));
    p.drawText(QRectF(area.left(),area.bottom()+5,area.width(),20),Qt::AlignRight,
      QString::number(x1,'g',7)+(view_==0?QStringLiteral(" UI"):QStringLiteral(" ps")));
    p.drawText(QRectF(area.left(),height()-23,area.width(),20),Qt::AlignCenter,
      view_==0?QStringLiteral("UI (2 symbols)"):QStringLiteral("Time (ps)"));
    p.save(); p.setClipRect(area);
    auto series=[&](const std::vector<double> &v,double xStep,double scale,QColor color) {
      if(v.size()<2)return;
      const size_t stride=std::max<size_t>(1,v.size()/3500);
      QPainterPath path;bool first=true;
      auto vertex=[&](size_t index) {
        double x=xp(double(index)*xStep), y=yp(v[index]*scale);
        if(first){path.moveTo(x,y);first=false;}else path.lineTo(x,y);
      };
      for(size_t i=0;i<v.size();i+=stride) {
        size_t mini=i,maxi=i;
        for(size_t j=i+1;j<std::min(v.size(),i+stride);++j) {
          if(v[j]<v[mini])mini=j;
          if(v[j]>v[maxi])maxi=j;
        }
        if(mini<=maxi){vertex(mini);if(maxi!=mini)vertex(maxi);}
        else {vertex(maxi);vertex(mini);}
      }
      vertex(v.size()-1);
      p.setPen(QPen(color,1.3));p.drawPath(path);
    };
    if(view_==0) {
      if(d.overlayPhaseUI.size()>1) {
        const double step=d.overlayPhaseUI[1]-d.overlayPhaseUI[0];
        for(const auto &v:d.overlayV)series(v,step,1000,QColor(38,126,197,95));
      }
      p.setPen(QPen(QColor("#c75b44"),1.5,Qt::DashLine));
      p.drawLine(QPointF(area.left(),yp(thresholdV_*1000)),
                 QPointF(area.right(),yp(thresholdV_*1000)));
      p.setPen(QPen(QColor("#128b76"),1.2,Qt::DashLine));
      for(double y:{d.minimumHighV,d.maximumLowV})
        p.drawLine(QPointF(area.left(),yp(y*1000)),QPointF(area.right(),yp(y*1000)));
      if(d.samplingPhaseThresholdOpen) {
        p.setPen(QPen(QColor("#aa6b24"),1.4,Qt::DashLine));
        for(double x:{d.samplingPhaseUI,d.samplingPhaseUI+1.0})
          p.drawLine(QPointF(xp(x),area.top()),QPointF(xp(x),area.bottom()));
      }
    } else if(view_==1) {
      series(d.transmittedV,d.timeStepSeconds*1e12,1000,QColor("#d47a24"));
      series(d.receivedV,d.timeStepSeconds*1e12,1000,QColor("#246cad"));
    } else {
      series(d.impulse.values,d.timeStepSeconds*1e12,1,QColor("#246cad"));
      series(d.unitPulse.values,d.timeStepSeconds*1e12,1,QColor("#d47a24"));
    }
    if(cursor_>=0) {
      p.setPen(QPen(palette().text().color(),1,Qt::DotLine));
      const double xx=area.left()+cursor_*area.width();
      p.drawLine(QPointF(xx,area.top()),QPointF(xx,area.bottom()));
    }
    p.restore();
  }
private:
  std::shared_ptr<const si::channel::EyeData> data_;
  double thresholdV_=0,zoom_=1,shift_=0,cursor_=-1,lastX_=0;
  int view_=0;
  bool dragging_=false;
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
  resize(feature_==Feature::Eye?1120:940, feature_==Feature::Eye?840:640);
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
    fromGhz_->setObjectName(QStringLiteral("ildStartGhz"));
    toGhz_->setObjectName(QStringLiteral("ildStopGhz"));
    fitModel_ = new QComboBox;
    fitModel_->setObjectName(QStringLiteral("ildFitModel"));
    fitModel_->addItems({QStringLiteral("Linear"), QStringLiteral("Root + linear"),
                         QStringLiteral("Root + linear + quadratic")});
    fitModel_->setCurrentIndex(1);
    form->addRow(QStringLiteral("ILD fit model"),fitModel_);
  }
  if(feature_ == Feature::Icn) {
    icnAggressors_=new QListWidget;
    icnAggressors_->setObjectName(QStringLiteral("icnAggressorList"));
    icnAggressors_->setMinimumHeight(100);
    icnIndependent_=new QCheckBox(QStringLiteral("I confirm selected aggressors are modeled as uncorrelated (RSS)"));
    icnIndependent_->setObjectName(QStringLiteral("icnIndependentAssumption"));
    icnIndependent_->setChecked(false);
    icnBaud_=new QDoubleSpinBox;
    icnBaud_->setObjectName(QStringLiteral("icnSymbolRate"));
    icnBaud_->setRange(0.001,1000);icnBaud_->setDecimals(3);
    icnBaud_->setSuffix(QStringLiteral(" GBd"));icnBaud_->setValue(4);
    icnVpp_=new QDoubleSpinBox;
    icnVpp_->setObjectName(QStringLiteral("icnTxVpp"));
    icnVpp_->setRange(0,100000);icnVpp_->setDecimals(3);
    icnVpp_->setSuffix(QStringLiteral(" mVpp"));icnVpp_->setValue(800);
    icnRise_=new QDoubleSpinBox;
    icnRise_->setObjectName(QStringLiteral("icnRiseTime"));
    icnRise_->setRange(0,100000);icnRise_->setDecimals(3);
    icnRise_->setSuffix(QStringLiteral(" ps (10–90%)"));icnRise_->setValue(30);
    icnRx_=new QDoubleSpinBox;
    icnRx_->setObjectName(QStringLiteral("icnRxBandwidth"));
    icnRx_->setRange(0.001,1000);icnRx_->setDecimals(3);
    icnRx_->setSuffix(QStringLiteral(" GHz"));icnRx_->setValue(10);
  }
  form->addRow(QStringLiteral("Source / Revision"), source_);
  form->addRow(QStringLiteral("Victim Channel"), victim_);
  if (feature_ == Feature::Com)
    form->addRow(QStringLiteral("Aggressor"), aggressor_);
  else aggressor_->hide();
  form->addRow(QStringLiteral("Direction"), direction_);
  if(feature_!=Feature::Eye) {
    form->addRow(QStringLiteral("Start frequency"), fromGhz_);
    form->addRow(QStringLiteral("Stop frequency"), toGhz_);
  } else {
    fromGhz_->setValue(0);
    toGhz_->setObjectName(QStringLiteral("eyeAnalysisCapGhz"));
    toGhz_->setValue(10);
    eyeProfile_=new QComboBox;
    eyeProfile_->setObjectName(QStringLiteral("eyeProfile"));
    for(int i=0;i<=int(si::channel::EyeProfile::Custom);++i)
      eyeProfile_->addItem(QString::fromUtf8(
        si::channel::eyeProfileName(static_cast<si::channel::EyeProfile>(i))));
    eyeMode_=new QComboBox;
    eyeMode_->setObjectName(QStringLiteral("eyeMode"));
    eyeMode_->addItems({QStringLiteral("Required: choose mode"),
                        QStringLiteral("Single-ended"),QStringLiteral("Differential")});
    eyeBitRate_=new QDoubleSpinBox;
    eyeBitRate_->setObjectName(QStringLiteral("eyeBitRateGbps"));
    eyeReference_=new QDoubleSpinBox;
    eyeReference_->setObjectName(QStringLiteral("eyeReferenceGhz"));
    for(auto *spin:{eyeBitRate_,eyeReference_}) {
      spin->setRange(0,1000);
      spin->setDecimals(9);
      spin->setSpecialValueText(QStringLiteral("Required"));
    }
    eyeBitRate_->setSuffix(QStringLiteral(" Gbps"));
    eyeReference_->setSuffix(QStringLiteral(" GHz"));
    eyePattern_=new QComboBox;
    eyePattern_->setObjectName(QStringLiteral("eyePattern"));
    eyePattern_->addItem(QStringLiteral("PRBS7"),int(si::channel::EyePattern::Prbs7));
    eyePattern_->addItem(QStringLiteral("Alternating"),int(si::channel::EyePattern::Alternating));
    eyePattern_->addItem(QStringLiteral("Custom bits"),int(si::channel::EyePattern::CustomBits));
    eyeBits_=new QLineEdit;
    eyeBits_->setObjectName(QStringLiteral("eyeCustomBits"));
    eyeBits_->setPlaceholderText(QStringLiteral("Binary sequence e.g. 00101110"));
    eyeBits_->setMaxLength(65536);
    eyeBits_->setEnabled(false);
    eyeFft_=new QComboBox;
    eyeFft_->setObjectName(QStringLiteral("eyeFftLength"));
    for(int n=1024;n<=65536;n*=2)eyeFft_->addItem(QString::number(n));
    eyeFft_->setCurrentText(QStringLiteral("8192"));
    eyeSamples_=new QComboBox;
    eyeSamples_->setObjectName(QStringLiteral("eyeSamplesPerUi"));
    for(int n=8;n<=128;n*=2)eyeSamples_->addItem(QString::number(n));
    eyeSamples_->setCurrentText(QStringLiteral("32"));
    eyeView_=new QComboBox;
    eyeView_->setObjectName(QStringLiteral("eyePlotView"));
    eyeView_->addItems({QStringLiteral("Eye overlay (2 UI)"),
                        QStringLiteral("TX / RX waveform"),
                        QStringLiteral("Impulse / Unit pulse")});
    const QStringList fields={
      QStringLiteral("TX incident low (mV)"),QStringLiteral("TX incident high (mV)"),
      QStringLiteral("TX source (ohm)"),QStringLiteral("RX termination (ohm)"),
      QStringLiteral("RX threshold (mV)"),QStringLiteral("RX input minimum (mV)"),
      QStringLiteral("RX input maximum (mV)")};
    const QStringList names={
      QStringLiteral("eyeTxLowMv"),QStringLiteral("eyeTxHighMv"),
      QStringLiteral("eyeTxSourceOhm"),QStringLiteral("eyeRxTerminationOhm"),
      QStringLiteral("eyeRxThresholdMv"),QStringLiteral("eyeRxMinimumMv"),
      QStringLiteral("eyeRxMaximumMv")};
    form->addRow(QStringLiteral("PHY profile (example, not compliance)"),eyeProfile_);
    form->addRow(QStringLiteral("Transfer mode"),eyeMode_);
    form->addRow(QStringLiteral("NRZ bit rate"),eyeBitRate_);
    form->addRow(QStringLiteral("NRZ Nyquist reference (coupled)"),eyeReference_);
    form->addRow(QStringLiteral("Independent analysis cap"),toGhz_);
    form->addRow(QStringLiteral("Pattern"),eyePattern_);
    form->addRow(QStringLiteral("Custom pattern"),eyeBits_);
    form->addRow(QStringLiteral("FFT samples"),eyeFft_);
    form->addRow(QStringLiteral("Samples per UI"),eyeSamples_);
    for(int i=0;i<7;++i) {
      eyeElectrical_[size_t(i)]=new QLineEdit;
      eyeElectrical_[size_t(i)]->setObjectName(names[i]);
      eyeElectrical_[size_t(i)]->setPlaceholderText(
        QStringLiteral("Required — physical reference-plane value"));
      form->addRow(fields[i],eyeElectrical_[size_t(i)]);
    }
    form->addRow(QStringLiteral("Graph view"),eyeView_);
  }
  if(feature_==Feature::Icn) {
    form->addRow(QStringLiteral("NEXT / FEXT paths (check multiple)"),icnAggressors_);
    form->addRow(QStringLiteral("Aggressor correlation"),icnIndependent_);
    form->addRow(QStringLiteral("NRZ symbol rate"),icnBaud_);
    form->addRow(QStringLiteral("TX amplitude, peak-to-peak"),icnVpp_);
    form->addRow(QStringLiteral("Gaussian rise time"),icnRise_);
    form->addRow(QStringLiteral("RX 3 dB bandwidth"),icnRx_);
  }
  if(feature_==Feature::Eye) {
    auto *panel=new QWidget;
    panel->setLayout(form);
    auto *scroll=new QScrollArea;
    scroll->setObjectName(QStringLiteral("eyeSettingsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setWidget(panel);
    scroll->setMinimumHeight(220);
    scroll->setMaximumHeight(340);
    outer->addWidget(scroll);
  } else outer->addLayout(form);
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
  } else if(feature_==Feature::Icn) {
    graphPlaceholder_->hide();
    icnGraph_=new IcnPlotWidget(this);
    outer->addWidget(icnGraph_,2);
    icnContributions_=new QLabel(QStringLiteral("Select NEXT/FEXT paths to display individual contributions."));
    icnContributions_->setObjectName(QStringLiteral("icnContributions"));
    icnContributions_->setWordWrap(true);
    outer->addWidget(icnContributions_);
  } else if(feature_==Feature::Eye) {
    graphPlaceholder_->hide();
    eyeGraph_=new EyePlotWidget(this);
    outer->addWidget(eyeGraph_,2);
    eyeMetrics_=new QLabel(QStringLiteral("Simulated Channel Eye — Not Measured / Not PHY Compliance"));
    eyeMetrics_->setObjectName(QStringLiteral("eyeMetrics"));
    eyeMetrics_->setWordWrap(true);
    outer->addWidget(eyeMetrics_);
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
  if(feature_==Feature::Icn) {
    connect(run_,&QPushButton::clicked,this,[this]{launchIcn();});
    connect(csv_,&QPushButton::clicked,this,[this]{exportIcnCsv();});
    connect(png_,&QPushButton::clicked,this,[this]{exportIcnPng();});
    connect(icnAggressors_,&QListWidget::itemChanged,this,[this]{invalidateRequest();});
    connect(icnIndependent_,&QCheckBox::toggled,this,[this]{invalidateRequest();});
    for(auto *spin:{icnBaud_,icnVpp_,icnRise_,icnRx_})
      connect(spin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
              this,[this](double){invalidateRequest();});
    connect(victim_,&QComboBox::currentIndexChanged,this,[this]{
      populateIcnAggressors();
    });
  }
  if(feature_==Feature::Eye) {
    connect(run_,&QPushButton::clicked,this,[this]{launchEye();});
    connect(csv_,&QPushButton::clicked,this,[this]{exportEyeCsv();});
    connect(png_,&QPushButton::clicked,this,[this]{exportEyePng();});
    connect(eyeProfile_,&QComboBox::currentIndexChanged,this,[this]{selectEyeProfile();});
    connect(eyeMode_,&QComboBox::currentIndexChanged,this,[this]{invalidateRequest();});
    connect(eyePattern_,&QComboBox::currentIndexChanged,this,[this]{
      eyeBits_->setEnabled(eyePattern_->currentIndex()==2); invalidateRequest();
    });
    connect(eyeBits_,&QLineEdit::textChanged,this,[this]{invalidateRequest();});
    connect(eyeBitRate_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
      this,[this](double rate){
        if(eyeFrequencySync_)return;
        eyeFrequencySync_=true;
        { QSignalBlocker block(eyeReference_); eyeReference_->setValue(rate/2); }
        eyeFrequencySync_=false;invalidateRequest();
      });
    connect(eyeReference_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
      this,[this](double ref){
        if(eyeFrequencySync_)return;
        eyeFrequencySync_=true;
        { QSignalBlocker block(eyeBitRate_); eyeBitRate_->setValue(ref*2); }
        eyeFrequencySync_=false;invalidateRequest();
      });
    for(auto *combo:{eyeFft_,eyeSamples_})
      connect(combo,&QComboBox::currentIndexChanged,this,[this]{invalidateRequest();});
    connect(eyeView_,&QComboBox::currentIndexChanged,this,[this](int view){
      if(eyeGraph_) static_cast<EyePlotWidget *>(eyeGraph_)->setView(view);
    });
    for(auto *edit:eyeElectrical_)
      connect(edit,&QLineEdit::textChanged,this,[this]{invalidateRequest();});
    selectEyeProfile();
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
    populateIcnAggressors();
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
  QString identity = item.sha256 + QLatin1Char('|') + item.revision + QLatin1Char('|') +
    item.mappingDigest + QLatin1Char('|') + victim_->currentText() +
    QLatin1Char('|') + aggressor_->currentText() + QLatin1Char('|') +
    QString::number(direction_->currentIndex()) + QLatin1Char('|') +
    QString::number(fromGhz_->value(), 'g', 17) + QLatin1Char('|') +
    QString::number(toGhz_->value(), 'g', 17) + QLatin1Char('|') +
    QString::number(fitModel_ ? fitModel_->currentIndex() : -1);
  if(feature_==Feature::Icn) {
    for(int k=0;k<icnAggressors_->count();++k) {
      auto *item=icnAggressors_->item(k);
      if(item->checkState()==Qt::Checked)
        identity+=QStringLiteral("|X")+QString::number(item->data(Qt::UserRole).toInt())+
                  QStringLiteral("/") + QString::number(item->data(Qt::UserRole+1).toInt());
    }
    identity+=icnIndependent_->isChecked()?QStringLiteral("|RSS_ACK"):QStringLiteral("|RSS_NOT_ACK");
    for(auto *spin:{icnBaud_,icnVpp_,icnRise_,icnRx_})
      identity+=QStringLiteral("|")+QString::number(spin->value(),'g',17);
  }
  if(feature_==Feature::Eye) {
    identity+=QStringLiteral("|EYE|")+QString::number(eyeProfile_->currentIndex())+
      QLatin1Char('|')+QString::number(eyeMode_->currentIndex())+
      QLatin1Char('|')+QString::number(eyePattern_->currentIndex())+
      QLatin1Char('|')+QString::number(eyeBitRate_->value(),'g',17)+
      QLatin1Char('|')+QString::number(eyeReference_->value(),'g',17)+
      QLatin1Char('|')+eyeFft_->currentText()+QLatin1Char('|')+
      eyeSamples_->currentText()+QLatin1Char('|')+eyeBits_->text();
    for(auto *edit:eyeElectrical_)
      identity+=QLatin1Char('|')+edit->text().trimmed();
  }
  return identity;
}
void AnalysisWindow::invalidateRequest() {
  clearIld();
  clearIcn();
  clearEye();
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
  clearIcn();
  clearEye();
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
  populateIcnAggressors();
  setStatus(hadWork || hadResult ? si::channel::Status::Stale
           : si::channel::Status::NotEvaluated,
           (feature_ == Feature::Ild && ildProvider_) ||
            (feature_ == Feature::Icn && icnProvider_) ||
             (feature_ == Feature::Eye && eyeProvider_) ?
             QStringLiteral("Context updated; select channels and run the verified engine") :
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
        guard->progress_->setValue((outcome.status == si::channel::Status::Valid ||
           outcome.status == si::channel::Status::Warning) ? 100 : 0);
        guard->setStatus(outcome.status, outcome.diagnostic);
        if (outcome.ild && (outcome.status == si::channel::Status::Valid ||
                            outcome.status == si::channel::Status::Warning))
          guard->showIld(outcome);
        if (outcome.icn && (outcome.status == si::channel::Status::Valid ||
                            outcome.status == si::channel::Status::Warning))
          guard->showIcn(outcome);
        if (outcome.eye && (outcome.status == si::channel::Status::Valid ||
                            outcome.status == si::channel::Status::Warning))
          guard->showEye(outcome);
      });
  } catch (const std::exception &e) {
    setStatus(si::channel::Status::Error, QString::fromUtf8(e.what()));
    return false;
  }
  clearIld();
  clearIcn();
  clearEye();
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
      identity.mappingIdentity,"Generic ILD bandwidth-weighted least-squares","012_44-v2",
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
  setStatus(outcome.status,QStringLiteral("Band RMS %1 dB | peak abs %2 dB @ %3 GHz | %4 samples | %5–%6 GHz. Bandwidth-weighted fit; no standard PASS")
    .arg(d.rmsDb,0,'g',6).arg(d.peakAbsoluteDb,0,'g',6)
    .arg(d.peakFrequencyHz/1e9,0,'g',7).arg(qulonglong(d.frequencyHz.size()))
    .arg(d.rangeStartHz/1e9,0,'g',7).arg(d.rangeStopHz/1e9,0,'g',7));
}
void AnalysisWindow::exportIldCsv() {
  if(!ildResult_) return;
  const QString name=QFileDialog::getSaveFileName(this,QStringLiteral("Export ILD data"),
                          sparamview::preferences::saveSuggestion(QStringLiteral("ILD.csv")),QStringLiteral("CSV (*.csv)"));
  if(!name.isEmpty() && saveIldCsv(name)) sparamview::preferences::rememberFile(name);
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
     <<"# requested_start_hz,"<<QString::number(d.requestedStartHz,'g',17)<<"\n"
     <<"# requested_stop_hz,"<<QString::number(d.requestedStopHz,'g',17)<<"\n"
     <<"# fit_weighting,trapezoidal_bandwidth_cell\n"
     <<"# rms_definition,band_integral_piecewise_linear\n"
     <<"# rms_db,"<<QString::number(d.rmsDb,'g',17)<<"\n"
     <<"# peak_abs_db,"<<QString::number(d.peakAbsoluteDb,'g',17)<<"\n"
     <<"# peak_frequency_hz,"<<QString::number(d.peakFrequencyHz,'g',17)<<"\n";
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
                       sparamview::preferences::saveSuggestion(QStringLiteral("ILD.png")),QStringLiteral("PNG (*.png)"));
  if(!name.isEmpty() && saveIldPng(name)) sparamview::preferences::rememberFile(name);
}
bool AnalysisWindow::saveIldPng(const QString &name) {
  if(!ildResult_ || !ildGraph_ || name.isEmpty()) return false;
  QSaveFile file(name);
  const bool okay=file.open(QIODevice::WriteOnly) &&
                   ildGraph_->grab().save(&file,"PNG") && file.commit();
  if(!okay) setStatus(si::channel::Status::Error,QStringLiteral("ILD PNG write failed"));
  return okay;
}

void AnalysisWindow::populateIcnAggressors() {
  if(!icnAggressors_) return;
  QSignalBlocker blocked(icnAggressors_);
  icnAggressors_->clear();
  const int idx=source_->currentIndex();
  if(idx<0 || idx>=context_.sources.size()) return;
  const auto &src=context_.sources[idx];
  const int victimIndex=victim_->currentIndex();
  if(victimIndex<0 || victimIndex>=src.channelIds.size()) return;
  for(int k=0;k<src.channelIds.size();++k) {
    if(k==victimIndex) continue;
    for(int end=0;end<2;++end) {
      auto *item=new QListWidgetItem(
        (end==0?QStringLiteral("NEXT — "):QStringLiteral("FEXT — "))+src.channels[k],
        icnAggressors_);
      item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
      item->setCheckState(Qt::Unchecked);
      item->setData(Qt::UserRole,k);
      item->setData(Qt::UserRole+1,end);
    }
  }
}
void AnalysisWindow::setIcnSourceProvider(IcnSourceProvider provider) {
  icnProvider_=std::move(provider);
  if(feature_==Feature::Icn) {
    run_->setEnabled(bool(icnProvider_));
    run_->setToolTip(icnProvider_?QStringLiteral("Run generic band-limited NRZ ICN")
      :QStringLiteral("ICN source provider unavailable"));
  }
}
void AnalysisWindow::clearIcn() {
  icnResult_.reset();
  if(icnGraph_) static_cast<IcnPlotWidget *>(icnGraph_)->setData({});
  if(icnContributions_) icnContributions_->setText(QStringLiteral("No current ICN result."));
  if(feature_==Feature::Icn) {
    csv_->setEnabled(false);
    png_->setEnabled(false);
  }
}
void AnalysisWindow::launchIcn() {
  if(feature_!=Feature::Icn || !icnProvider_) return;
  const int idx=source_->currentIndex();
  if(idx<0 || idx>=context_.sources.size() || victim_->currentIndex()<0 ||
     fromGhz_->value()>=toGhz_->value()) {
    setStatus(si::channel::Status::InvalidSource,
              QStringLiteral("Select confirmed source and frequency band"));
    return;
  }
  const auto &source=context_.sources[idx];
  const int victimIndex=victim_->currentIndex();
  if(!source.mappingConfirmed || victimIndex>=source.channelIds.size()) {
    setStatus(si::channel::Status::InvalidSource,
              QStringLiteral("Missing confirmed physical victim identity"));
    return;
  }
  std::vector<std::pair<std::string,si::channel::IcnEnd>> chosen;
  for(int k=0;k<icnAggressors_->count();++k) {
    auto *item=icnAggressors_->item(k);
    if(item->checkState()!=Qt::Checked) continue;
    const int index=item->data(Qt::UserRole).toInt();
    const int end=item->data(Qt::UserRole+1).toInt();
    if(index<0 || index>=source.channelIds.size() || index==victimIndex ||
       (end!=0 && end!=1)) {
      setStatus(si::channel::Status::InvalidSource,QStringLiteral("Invalid aggressor mapping"));
      return;
    }
    chosen.emplace_back(source.channelIds[index].toStdString(),
        end==0?si::channel::IcnEnd::Next:si::channel::IcnEnd::Fext);
  }
  if(chosen.empty() || chosen.size()>32) {
    setStatus(si::channel::Status::NotEvaluated,
              QStringLiteral("Select 1–32 NEXT/FEXT aggressor paths"));
    return;
  }
  if(chosen.size()>1 && !icnIndependent_->isChecked()) {
    setStatus(si::channel::Status::NotEvaluated,
      QStringLiteral("Multiple aggressors require explicitly confirming the uncorrelated RSS assumption"));
    return;
  }
  const auto input=icnProvider_(idx);
  if(!input || !input->cache ||
     input->revision!=source.revision.toStdString() ||
     input->expectedSha!=source.sha256.toStdString()) {
    setStatus(si::channel::Status::InvalidSource,
              QStringLiteral("Source revision changed; refresh analysis"));
    return;
  }
  const std::string victimId=source.channelIds[victimIndex].toStdString();
  const bool reverse=direction_->currentIndex()==1;
  si::channel::IcnSettings settings;
  settings.startHz=fromGhz_->value()*1e9;
  settings.stopHz=toGhz_->value()*1e9;
  settings.symbolRateHz=icnBaud_->value()*1e9;
  settings.txPeakToPeakV=icnVpp_->value()*1e-3;
  settings.riseTimeSeconds=icnRise_->value()*1e-12;
  settings.rxBandwidthHz=icnRx_->value()*1e9;
  const std::string settingsIdentity=selectionIdentity().toStdString();
  startWork([input=*input,victimId,reverse,chosen,settings,settingsIdentity]
      (const std::atomic_bool &cancel,std::atomic_int &progress)->WorkOutcome {
    if(cancel.load()) throw si::Cancelled();
    progress.store(10);
    auto snap=si::channel::SourceSnapshot::capture(
      input.cache,input.mapping,input.revision,
      input.workspaceGeneration,input.mappingGeneration);
    if(snap.identity().sha256!=input.expectedSha)
      throw si::Error("ICN source SHA mismatch");
    const auto &victim=snap.channel(victimId);
    std::vector<si::channel::IcnAggressorInput> paths;
    paths.reserve(chosen.size());
    for(size_t i=0;i<chosen.size();++i) {
      if(cancel.load()) throw si::Cancelled();
      const auto &a=snap.channel(chosen[i].first);
      if(a.id==victim.id || a.group!=victim.group ||
         a.differential()!=victim.differential())
        throw si::Error("ICN aggressor must be a different, compatible channel in the victim group");
      const auto metric=chosen[i].second==si::channel::IcnEnd::Next ?
                         si::Metric::NEXT:si::Metric::FEXT;
      auto trace=si::channel::extract(snap,victimId,metric,reverse,
                 std::string_view(chosen[i].first));
      paths.push_back({chosen[i].first,chosen[i].second,std::move(trace)});
      progress.store(15+int(50*(i+1)/chosen.size()));
    }
    const auto &identity=snap.identity();
    si::channel::Provenance provenance{identity.sha256,identity.revision,
      identity.mappingIdentity,"Generic uncorrelated NRZ ICN","012_45-v2",
      settingsIdentity,identity.workspaceGeneration,identity.mappingGeneration};
    auto result=si::channel::calculateIcn(paths,settings,provenance,&cancel);
    result.value->victimId=victimId;
    result.value->direction=reverse?"reverse":"forward";
    if(cancel.load()) throw si::Cancelled();
    progress.store(100);
    WorkOutcome outcome;
    outcome.status=result.status;
    outcome.diagnostic=QStringLiteral("Generic NRZ band-limited ICN; not IEEE/SNIA compliance");
    outcome.icn=std::make_shared<si::channel::IcnData>(std::move(*result.value));
    outcome.provenance=std::move(result.provenance);
    return outcome;
  });
}
void AnalysisWindow::showIcn(const WorkOutcome &outcome) {
  if(!outcome.icn || !icnGraph_) return;
  icnResult_=outcome.icn;
  icnProvenance_=outcome.provenance;
  static_cast<IcnPlotWidget *>(icnGraph_)->setData(icnResult_);
  csv_->setEnabled(true);
  png_->setEnabled(true);
  const auto &d=*icnResult_;
  QStringList contributions;
  for(size_t i=0;i<d.aggressors.size();++i) {
    const auto &a=d.aggressors[i];
    contributions<<QStringLiteral("Path %1 %2 %3: %4 mV RMS (%5% power)")
      .arg(qulonglong(i)).arg(a.end==si::channel::IcnEnd::Next?
          QStringLiteral("NEXT"):QStringLiteral("FEXT"))
      .arg(QString::fromStdString(a.id)).arg(a.rmsV*1000,0,'g',6)
      .arg(a.powerPercent,0,'g',5);
  }
  if(icnContributions_) icnContributions_->setText(contributions.join(QStringLiteral(" | ")));
  setStatus(outcome.status,QStringLiteral(
    "NEXT %1 mV RMS | FEXT %2 mV RMS | Total %3 mV RMS | %4 paths%5 | %6–%7 GHz — Generic independent-path NRZ RSS, no standard PASS")
    .arg(d.nextRmsV*1000,0,'g',7).arg(d.fextRmsV*1000,0,'g',7)
    .arg(d.totalRmsV*1000,0,'g',7).arg(qulonglong(d.aggressors.size()))
    .arg(d.bandLimited?QStringLiteral(" | BAND LIMITED"):QString())
    .arg(d.actualStartHz/1e9,0,'g',7).arg(d.actualStopHz/1e9,0,'g',7));
}
void AnalysisWindow::exportIcnCsv() {
  if(!icnResult_) return;
  const QString name=QFileDialog::getSaveFileName(this,
    QStringLiteral("Export ICN numerical data"),sparamview::preferences::saveSuggestion(QStringLiteral("ICN.csv")),
    QStringLiteral("CSV (*.csv)"));
  if(!name.isEmpty() && saveIcnCsv(name)) sparamview::preferences::rememberFile(name);
}
bool AnalysisWindow::saveIcnCsv(const QString &name) {
  if(!icnResult_ || name.isEmpty() ||
     (status_!=si::channel::Status::Valid &&
      status_!=si::channel::Status::Warning)) return false;
  QSaveFile file(name);
  if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
    setStatus(si::channel::Status::Error,QStringLiteral("Cannot open ICN CSV"));
    return false;
  }
  QTextStream out(&file);
  const auto &d=*icnResult_;
  auto safe=[](const std::string &raw) {
    QString v=QString::fromStdString(raw);
    v.replace(QLatin1Char('\r'),QLatin1Char(' '));
    v.replace(QLatin1Char('\n'),QLatin1Char(' '));
    v.replace(QStringLiteral("\""),QStringLiteral("\"\""));
    const QString trimmed=v.trimmed();
    if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
      v.prepend(QLatin1Char('\''));
    return QStringLiteral("\"")+v+QStringLiteral("\"");
  };
  const auto n=[](double v){return QString::number(v,'g',17);};
  out<<"# source_sha256,"<<safe(icnProvenance_.sourceSha256)<<"\n"
     <<"# revision,"<<safe(icnProvenance_.revision)<<"\n"
     <<"# mapping,"<<safe(icnProvenance_.mappingIdentity)<<"\n"
     <<"# settings_identity,"<<safe(icnProvenance_.settingsIdentity)<<"\n"
     <<"# algorithm,"<<safe(icnProvenance_.algorithm)<<"\n"
     <<"# version,"<<safe(icnProvenance_.version)<<"\n"
     <<"# model,"<<safe(d.model)<<"\n"
     <<"# victim_id,"<<safe(d.victimId)<<"\n"
     <<"# direction,"<<safe(d.direction)<<"\n"
     <<"# correlation_assumption,independent_path_rss\n"
     <<"# requested_start_hz,"<<n(d.settings.startHz)<<"\n"
     <<"# requested_stop_hz,"<<n(d.settings.stopHz)<<"\n"
     <<"# symbol_rate_baud,"<<n(d.settings.symbolRateHz)<<"\n"
     <<"# tx_peak_to_peak_v,"<<n(d.settings.txPeakToPeakV)<<"\n"
     <<"# rise_time_10_90_s,"<<n(d.settings.riseTimeSeconds)<<"\n"
     <<"# rx_bandwidth_hz,"<<n(d.settings.rxBandwidthHz)<<"\n"
     <<"# workspace_generation,"<<qulonglong(icnProvenance_.workspaceGeneration)<<"\n"
     <<"# mapping_generation,"<<qulonglong(icnProvenance_.mappingGeneration)<<"\n"
     <<"# band_limited,"<<(d.bandLimited?"true":"false")<<"\n"
     <<"# actual_start_hz,"<<n(d.actualStartHz)<<"\n"
     <<"# actual_stop_hz,"<<n(d.actualStopHz)<<"\n"
     <<"# next_rms_mV,"<<n(d.nextRmsV*1000)<<"\n"
     <<"# fext_rms_mV,"<<n(d.fextRmsV*1000)<<"\n"
     <<"# total_rms_mV,"<<n(d.totalRmsV*1000)<<"\n";
  for(size_t k=0;k<d.aggressors.size();++k) {
    const auto &a=d.aggressors[k];
    out<<"# path_"<<k<<"_id,"<<safe(a.id)<<"\n"
       <<"# path_"<<k<<"_type,"<<(a.end==si::channel::IcnEnd::Next?"NEXT":"FEXT")<<"\n"
       <<"# path_"<<k<<"_parameter,"<<safe(a.parameter)<<"\n"
       <<"# path_"<<k<<"_rms_mV,"<<n(a.rmsV*1000)<<"\n"
       <<"# path_"<<k<<"_power_percent,"<<n(a.powerPercent)<<"\n";
  }
  out<<"Frequency_GHz,NEXT_Power_dB,FEXT_Power_dB,"
        "NEXT_PSD_V2_per_Hz,FEXT_PSD_V2_per_Hz,"
        "NEXT_Cumulative_RMS_mV,FEXT_Cumulative_RMS_mV,Total_Cumulative_RMS_mV";
  for(size_t k=0;k<d.aggressors.size();++k)
    out<<",Path"<<k<<"_PSD_V2_per_Hz,Path"<<k<<"_Cumulative_RMS_mV";
  out<<"\n";
  for(size_t i=0;i<d.frequencyHz.size();++i) {
    out<<n(d.frequencyHz[i]/1e9)<<","<<n(d.nextPowerDb[i])<<","
       <<n(d.fextPowerDb[i])<<","<<n(d.nextDensityV2Hz[i])<<","
       <<n(d.fextDensityV2Hz[i])<<","<<n(d.nextCumulativeRmsV[i]*1000)
       <<","<<n(d.fextCumulativeRmsV[i]*1000)<<","
       <<n(d.totalCumulativeRmsV[i]*1000);
    for(const auto &a:d.aggressors)
      out<<","<<n(a.spectralDensityV2Hz[i])<<","<<n(a.cumulativeRmsV[i]*1000);
    out<<"\n";
  }
  out.flush();
  const bool okay=out.status()==QTextStream::Ok && file.commit();
  if(!okay) setStatus(si::channel::Status::Error,QStringLiteral("ICN CSV write failed"));
  return okay;
}
void AnalysisWindow::exportIcnPng() {
  if(!icnResult_) return;
  const QString name=QFileDialog::getSaveFileName(this,
    QStringLiteral("Export ICN plot"),sparamview::preferences::saveSuggestion(QStringLiteral("ICN.png")),
    QStringLiteral("PNG (*.png)"));
  if(!name.isEmpty() && saveIcnPng(name)) sparamview::preferences::rememberFile(name);
}
bool AnalysisWindow::saveIcnPng(const QString &name) {
  if(!icnResult_ || !icnGraph_ || name.isEmpty() ||
     (status_!=si::channel::Status::Valid &&
      status_!=si::channel::Status::Warning)) return false;
  QSaveFile file(name);
  const bool okay=file.open(QIODevice::WriteOnly) &&
                   icnGraph_->grab().save(&file,"PNG") && file.commit();
  if(!okay) setStatus(si::channel::Status::Error,QStringLiteral("ICN PNG write failed"));
  return okay;
}

void AnalysisWindow::selectEyeProfile() {
  if(feature_!=Feature::Eye || !eyeProfile_)return;
  const auto profile=static_cast<si::channel::EyeProfile>(eyeProfile_->currentIndex());
  const auto s=si::channel::eyeProfile(profile);
  {
    QSignalBlocker b1(eyeBitRate_),b2(eyeReference_),b3(toGhz_),b4(eyeMode_);
    QSignalBlocker b5(eyePattern_),b6(eyeBits_);
    eyeBitRate_->setValue(std::isfinite(s.bitRateHz)?s.bitRateHz/1e9:0);
    eyeReference_->setValue(std::isfinite(s.referenceFrequencyHz)?
                            s.referenceFrequencyHz/1e9:0);
    toGhz_->setValue(std::isfinite(s.analysisStopHz)?s.analysisStopHz/1e9:0);
    eyeMode_->setCurrentIndex(s.mode==si::channel::EyeMode::Differential?2:0);
    eyePattern_->setCurrentIndex(0);
    eyeBits_->clear();
    eyeBits_->setEnabled(false);
    for(auto *edit:eyeElectrical_) {
      QSignalBlocker electricalBlock(edit);
      edit->clear(); // PHY labels NEVER infer voltage / actual termination.
    }
  }
  invalidateRequest();
}
void AnalysisWindow::setEyeSourceProvider(EyeSourceProvider provider) {
  eyeProvider_=std::move(provider);
  if(feature_==Feature::Eye) {
    run_->setEnabled(bool(eyeProvider_));
    run_->setToolTip(eyeProvider_?QStringLiteral(
      "Run phase-preserving simulated NRZ Eye with explicit electrical conditions")
      :QStringLiteral("Verified Eye source provider unavailable"));
  }
}
void AnalysisWindow::clearEye() {
  eyeResult_.reset();
  eyeSourceFilename_.clear();
  if(eyeGraph_) static_cast<EyePlotWidget *>(eyeGraph_)->setData({},0);
  if(eyeMetrics_) eyeMetrics_->setText(QStringLiteral(
    "Simulated Channel Eye — Not Measured / Not PHY Compliance"));
  if(feature_==Feature::Eye && csv_ && png_) {
    csv_->setEnabled(false); png_->setEnabled(false);
  }
}
void AnalysisWindow::launchEye() {
  using namespace si::channel;
  if(feature_!=Feature::Eye || !eyeProvider_)return;
  clearEye();
  const int idx=source_->currentIndex(), selected=victim_->currentIndex();
  if(idx<0 || idx>=context_.sources.size() || selected<0) {
    setStatus(Status::InvalidSource,QStringLiteral("Select a verified Touchstone revision and channel"));
    return;
  }
  const auto &source=context_.sources[idx];
  if(!source.mappingConfirmed || selected>=source.channelIds.size()) {
    setStatus(Status::InvalidSource,QStringLiteral("Confirmed physical port mapping required"));
    return;
  }
  const auto input=eyeProvider_(idx);
  if(!input || !input->cache || input->expectedSha!=source.sha256.toStdString() ||
     input->revision!=source.revision.toStdString()) {
    setStatus(Status::InvalidSource,QStringLiteral("Source SHA/revision changed; refresh analysis"));
    return;
  }
  if(eyeMode_->currentIndex()==0 || eyeBitRate_->value()<=0 ||
     eyeReference_->value()<=0 || toGhz_->value()<=0) {
    setStatus(Status::NotEvaluated,QStringLiteral(
      "Explicit signal mode, bit rate, reference frequency and analysis cap required"));
    return;
  }
  EyeSettings settings=eyeProfile(
    static_cast<EyeProfile>(eyeProfile_->currentIndex()));
  settings.mode=static_cast<EyeMode>(eyeMode_->currentIndex());
  settings.pattern=static_cast<EyePattern>(eyePattern_->currentData().toInt());
  settings.bitRateHz=eyeBitRate_->value()*1e9;
  settings.referenceFrequencyHz=eyeReference_->value()*1e9;
  settings.analysisStopHz=toGhz_->value()*1e9;
  settings.fftLength=size_t(eyeFft_->currentText().toULongLong());
  settings.samplesPerUI=size_t(eyeSamples_->currentText().toULongLong());
  if(settings.pattern==EyePattern::CustomBits) {
    const QByteArray bits=eyeBits_->text().trimmed().toLatin1();
    settings.customBits.reserve(size_t(bits.size()));
    for(const char ch:bits) {
      if(ch!='0' && ch!='1') {
        setStatus(Status::NotEvaluated,QStringLiteral("Custom pattern must contain only 0 and 1"));
        return;
      }
      settings.customBits.push_back(uint8_t(ch-'0'));
    }
  }
  std::array<double,7> electrical{};
  for(size_t i=0;i<electrical.size();++i) {
    const QString raw=eyeElectrical_[i]->text().trimmed();
    if(raw.isEmpty()) {
      setStatus(Status::NotEvaluated,QStringLiteral(
        "All TX/RX incident voltages, source/termination and RX range are required"));
      return;
    }
    bool ok=false;
    electrical[i]=raw.toDouble(&ok);
    if(!ok || !std::isfinite(electrical[i])) {
      setStatus(Status::Error,QStringLiteral("Invalid finite TX/RX electrical input"));
      return;
    }
  }
  settings.electrical={electrical[0]*1e-3,electrical[1]*1e-3,
    electrical[2],electrical[3],electrical[4]*1e-3,
    electrical[5]*1e-3,electrical[6]*1e-3};
  const QString sourceFile=QFileInfo(QString::fromUtf8(input->cache->meta().source.c_str())).fileName();
  const std::string victimId=source.channelIds[selected].toStdString();
  const bool reverse=direction_->currentIndex()==1;
  const std::string settingsIdentity=selectionIdentity().toStdString();
  startWork([input=*input,victimId,reverse,settings,settingsIdentity,sourceFile]
       (const std::atomic_bool &cancel,std::atomic_int &progress)->WorkOutcome {
    if(cancel.load()) throw si::Cancelled();
    progress.store(5);
    auto snapshot=SourceSnapshot::capture(input.cache,input.mapping,
        input.revision,input.workspaceGeneration,input.mappingGeneration);
    if(snapshot.identity().sha256!=input.expectedSha)
      throw si::Error("Eye source SHA changed since selection");
    if(snapshot.channel(victimId).differential() !=
       (settings.mode==EyeMode::Differential))
      throw si::Error("Eye single-ended/differential mode disagrees with mapped channel");
    if(cancel.load()) throw si::Cancelled();
    progress.store(35);
    const auto trace=extract(snapshot,victimId,si::Metric::IL,reverse);
    if(cancel.load()) throw si::Cancelled();
    progress.store(60);
    const auto &id=snapshot.identity();
    Provenance provenance{id.sha256,id.revision,id.mappingIdentity,
      "Phase-preserving deterministic NRZ Eye","012_46-v2",
      settingsIdentity,id.workspaceGeneration,id.mappingGeneration};
    auto result=calculateEye(trace,settings,provenance,&cancel);
    if(cancel.load()) throw si::Cancelled();
    WorkOutcome outcome;
    outcome.status=result.status;
    QStringList diagnostics;
    for(const auto &d:result.diagnostics)
      diagnostics<<QString::fromStdString(d.code);
    outcome.diagnostic=QStringLiteral(
      "Simulated only — Not measured, no BER or PHY compliance; ")+
      diagnostics.join(QStringLiteral(", "));
    if(result.usable()) {
      result.value->victimId=victimId;
      outcome.eye=std::make_shared<EyeData>(std::move(*result.value));
    }
    outcome.eyeSourceFilename=sourceFile;
    outcome.provenance=std::move(result.provenance);
    progress.store(100);
    return outcome;
  });
}
void AnalysisWindow::showEye(const WorkOutcome &outcome) {
  if(!outcome.eye || !eyeGraph_)return;
  eyeResult_=outcome.eye;
  eyeProvenance_=outcome.provenance;
  eyeSourceFilename_=outcome.eyeSourceFilename;
  auto *plot=static_cast<EyePlotWidget *>(eyeGraph_);
  plot->setData(eyeResult_,eyeResult_->settings.electrical.rxThresholdV);
  plot->setView(eyeView_->currentIndex());
  const auto &d=*eyeResult_;
  eyeMetrics_->setText(QStringLiteral(
    "Simulated Channel Eye — Not Measured / Not PHY Compliance\n"
    "Height %1 mV | Width %2 ps (%3 UI) | Max-height phase %4 UI | Delay %5 ps\n"
    "Low maximum %6 mV | High minimum %7 mV | Received [%8, %9] mV\n"
    "Bit rate %10 Gbps | Reference %11 GHz | Analysis cap %12 GHz")
    .arg(d.eyeHeightV*1000,0,'g',7).arg(d.eyeWidthSeconds*1e12,0,'g',7)
    .arg(d.eyeWidthUI,0,'g',7).arg(d.samplingPhaseUI,0,'g',7)
    .arg(d.estimatedDelaySeconds*1e12,0,'g',7)
    .arg(d.maximumLowV*1000,0,'g',7).arg(d.minimumHighV*1000,0,'g',7)
    .arg(d.minimumReceivedV*1000,0,'g',7).arg(d.maximumReceivedV*1000,0,'g',7)
    .arg(d.bitRateHz/1e9,0,'g',7).arg(d.referenceFrequencyHz/1e9,0,'g',7)
    .arg(d.analysisStopHz/1e9,0,'g',7)+
    QStringLiteral("\n%1 | %2 | Frame %3 bits | Overlay %4 traces | Width grid 1/%5 UI | Phase threshold: %6")
      .arg(QString::fromStdString(d.transferParameter))
      .arg(d.reverse?QStringLiteral("Reverse"):QStringLiteral("Forward"))
      .arg(qulonglong(d.frameBitCount)).arg(qulonglong(d.overlayV.size()))
      .arg(qulonglong(d.settings.samplesPerUI))
      .arg(d.samplingPhaseThresholdOpen?QStringLiteral("OPEN"):QStringLiteral("NOT OPEN")));
  csv_->setEnabled(true);png_->setEnabled(true);
  setStatus(outcome.status,outcome.diagnostic);
}
void AnalysisWindow::exportEyeCsv() {
  if(!eyeResult_)return;
  const QString name=QFileDialog::getSaveFileName(this,
      QStringLiteral("Export simulated Eye numerical data"),sparamview::preferences::saveSuggestion(QStringLiteral("Eye.csv")),
      QStringLiteral("CSV (*.csv)"));
  if(!name.isEmpty() && saveEyeCsv(name)) sparamview::preferences::rememberFile(name);
}
bool AnalysisWindow::saveEyeCsv(const QString &name) {
  using S=si::channel::Status;
  if(!eyeResult_ || name.isEmpty() ||
    (status_!=S::Warning && status_!=S::Valid))return false;
  QSaveFile file(name);
  if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
    setStatus(S::Error,QStringLiteral("Cannot open Eye CSV"));return false;
  }
  QTextStream out(&file);
  const auto &d=*eyeResult_;
  const auto &eyeSettings=d.settings;
  std::string customBits;
  for(auto bit:eyeSettings.customBits) customBits.push_back(bit?'1':'0');
  auto safe=[](const std::string &s) {
    QString v=QString::fromStdString(s);
    v.replace(QLatin1Char('\r'),QLatin1Char(' '));
    v.replace(QLatin1Char('\n'),QLatin1Char(' '));
    v.replace(QStringLiteral("\""),QStringLiteral("\"\""));
    const QString trimmed=v.trimmed();
    if(!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front()))
      v.prepend(QLatin1Char('\''));
    return QStringLiteral("\"")+v+QStringLiteral("\"");
  };
  const auto n=[](double v){return QString::number(v,'g',17);};
  out<<"# warning,\"Simulated Channel Eye — Not Measured / Not PHY Compliance\"\n"
     <<"# source_filename,"<<safe(eyeSourceFilename_.toStdString())<<"\n"
     <<"# source_sha256,"<<safe(eyeProvenance_.sourceSha256)<<"\n"
     <<"# revision,"<<safe(eyeProvenance_.revision)<<"\n"
     <<"# mapping_identity,"<<safe(eyeProvenance_.mappingIdentity)<<"\n"
     <<"# algorithm,"<<safe(eyeProvenance_.algorithm)<<"\n"
     <<"# version,"<<safe(eyeProvenance_.version)<<"\n"
     <<"# settings_identity,"<<safe(eyeProvenance_.settingsIdentity)<<"\n"
     <<"# transfer_parameter,"<<safe(d.transferParameter)<<"\n"
     <<"# victim_id,"<<safe(d.victimId)<<"\n"
     <<"# direction,"<<(d.reverse?"Reverse":"Forward")<<"\n"
     <<"# frame_bits,"<<d.frameBitCount<<"\n"
     <<"# overlay_count,"<<d.overlayV.size()<<"\n"
     <<"# custom_bits,"<<safe(customBits)<<"\n"
     <<"# width_resolution_ui,"<<n(1.0/double(eyeSettings.samplesPerUI))<<"\n"
     <<"# sampling_phase_threshold_open,"<<(d.samplingPhaseThresholdOpen?"true":"false")<<"\n"
     <<"# frame_model,PERIODIC_FRAME\n"
     <<"# workspace_generation,"<<eyeProvenance_.workspaceGeneration<<"\n"
     <<"# mapping_generation,"<<eyeProvenance_.mappingGeneration<<"\n"
     <<"# profile,"<<safe(d.profileName)<<"\n"
     <<"# bit_rate_gbps,"<<n(d.bitRateHz/1e9)<<"\n"
     <<"# reference_frequency_ghz,"<<n(d.referenceFrequencyHz/1e9)<<"\n"
     <<"# analysis_stop_ghz,"<<n(d.analysisStopHz/1e9)<<"\n"
     <<"# fft_length,"<<eyeSettings.fftLength<<"\n"
     <<"# samples_per_ui,"<<eyeSettings.samplesPerUI<<"\n"
     <<"# pattern,"<<int(eyeSettings.pattern)<<"\n"
     <<"# mode,"<<int(eyeSettings.mode)<<"\n"
     <<"# tx_low_incident_mv,"<<n(eyeSettings.electrical.txLowV*1000)<<"\n"
     <<"# tx_high_incident_mv,"<<n(eyeSettings.electrical.txHighV*1000)<<"\n"
     <<"# tx_source_ohm,"<<n(eyeSettings.electrical.txSourceOhm)<<"\n"
     <<"# rx_termination_ohm,"<<n(eyeSettings.electrical.rxTerminationOhm)<<"\n"
     <<"# rx_threshold_mv,"<<n(eyeSettings.electrical.rxThresholdV*1000)<<"\n"
     <<"# rx_input_min_mv,"<<n(eyeSettings.electrical.rxInputMinimumV*1000)<<"\n"
     <<"# rx_input_max_mv,"<<n(eyeSettings.electrical.rxInputMaximumV*1000)<<"\n"
     <<"# eye_height_mv,"<<n(d.eyeHeightV*1000)<<"\n"
     <<"# eye_width_ui,"<<n(d.eyeWidthUI)<<"\n"
     <<"# eye_width_ps,"<<n(d.eyeWidthSeconds*1e12)<<"\n"
     <<"# sampling_phase_ui,"<<n(d.samplingPhaseUI)<<"\n"
     <<"# estimated_delay_ps,"<<n(d.estimatedDelaySeconds*1e12)<<"\n"
     <<"# high_min_mv,"<<n(d.minimumHighV*1000)<<"\n"
     <<"# low_max_mv,"<<n(d.maximumLowV*1000)<<"\n"
     <<"# min_received_mv,"<<n(d.minimumReceivedV*1000)<<"\n"
     <<"# max_received_mv,"<<n(d.maximumReceivedV*1000)<<"\n"
     <<"# band_limited,"<<(d.bandLimited?"true":"false")<<"\n"
     <<"# status,SIMULATED_ONLY\n"
     <<"Time_ps,TX_mV,RX_mV,Impulse_Unit,Unit_Pulse\n";
  for(size_t i=0;i<d.timeSeconds.size();++i)
    out<<n(d.timeSeconds[i]*1e12)<<","<<n(d.transmittedV[i]*1000)
       <<","<<n(d.receivedV[i]*1000)<<","<<n(d.impulse.values[i])
       <<","<<n(d.unitPulse.values[i])<<"\n";
  out<<"Overlay_Index,Phase_UI,Received_mV\n";
  for(size_t k=0;k<d.overlayV.size();++k)
    for(size_t i=0;i<d.overlayV[k].size();++i)
      out<<k<<","<<n(d.overlayPhaseUI[i])<<","<<n(d.overlayV[k][i]*1000)<<"\n";
  out.flush();
  const bool okay=out.status()==QTextStream::Ok && file.commit();
  if(!okay)setStatus(S::Error,QStringLiteral("Eye CSV write/commit failed"));
  return okay;
}
void AnalysisWindow::exportEyePng() {
  if(!eyeResult_)return;
  const QString name=QFileDialog::getSaveFileName(this,
      QStringLiteral("Export simulated Eye graph"),sparamview::preferences::saveSuggestion(QStringLiteral("Eye.png")),
      QStringLiteral("PNG (*.png)"));
  if(!name.isEmpty() && saveEyePng(name)) sparamview::preferences::rememberFile(name);
}
bool AnalysisWindow::saveEyePng(const QString &name) {
  using S=si::channel::Status;
  if(!eyeResult_ || !eyeGraph_ || name.isEmpty() ||
     (status_!=S::Warning && status_!=S::Valid))return false;
  const QImage graph=eyeGraph_->grab().toImage();
  if(graph.isNull()) {
    setStatus(S::Error,QStringLiteral("Eye graph cannot be rendered"));
    return false;
  }
  // Visible footer plus PNG tEXt metadata make standalone exports auditable.
  constexpr int footerHeight=116;
  QImage output(graph.width(),graph.height()+footerHeight,
                QImage::Format_ARGB32_Premultiplied);
  output.fill(palette().base().color());
  QPainter p(&output);
  p.drawImage(0,0,graph);
  p.setPen(palette().text().color());
  QFont font=p.font();
  font.setPointSizeF(8);
  p.setFont(font);
  const QString caption=QStringLiteral(
    "Simulated Channel Eye — Not Measured / Not PHY Compliance\n"
    "File: %1 | Revision: %2 | Channel: %3 | %4\n"
    "SHA-256: %5\n"
    "Profile: %6 | Bit rate: %7 Gbps | Reference: %8 GHz | Cap: %9 GHz");
  const QString labeled=caption.arg(eyeSourceFilename_,
     QString::fromStdString(eyeProvenance_.revision),
     QString::fromStdString(eyeResult_->victimId),
     eyeResult_->reverse?QStringLiteral("Reverse"):QStringLiteral("Forward"),
     QString::fromStdString(eyeProvenance_.sourceSha256),
     QString::fromStdString(eyeResult_->profileName),
     QString::number(eyeResult_->bitRateHz/1e9,'g',7),
     QString::number(eyeResult_->referenceFrequencyHz/1e9,'g',7),
     QString::number(eyeResult_->analysisStopHz/1e9,'g',7));
  p.drawText(QRect(10,graph.height()+4,output.width()-20,footerHeight-8),
             Qt::AlignLeft|Qt::TextWordWrap,labeled);
  p.end();
  output.setText(QStringLiteral("SourceSHA256"),
                 QString::fromStdString(eyeProvenance_.sourceSha256));
  output.setText(QStringLiteral("SourceFile"),eyeSourceFilename_);
  output.setText(QStringLiteral("Revision"),
                 QString::fromStdString(eyeProvenance_.revision));
  output.setText(QStringLiteral("MappingIdentity"),
                 QString::fromStdString(eyeProvenance_.mappingIdentity));
  output.setText(QStringLiteral("SettingsIdentity"),
                 QString::fromStdString(eyeProvenance_.settingsIdentity));
  output.setText(QStringLiteral("Algorithm"),
                 QString::fromStdString(eyeProvenance_.algorithm));
  output.setText(QStringLiteral("AlgorithmVersion"),
                 QString::fromStdString(eyeProvenance_.version));
  output.setText(QStringLiteral("TransferParameter"),QString::fromStdString(eyeResult_->transferParameter));
  output.setText(QStringLiteral("VictimId"),QString::fromStdString(eyeResult_->victimId));
  output.setText(QStringLiteral("Direction"),eyeResult_->reverse?QStringLiteral("Reverse"):QStringLiteral("Forward"));
  output.setText(QStringLiteral("FrameBits"),QString::number(qulonglong(eyeResult_->frameBitCount)));
  output.setText(QStringLiteral("PhaseThresholdOpen"),eyeResult_->samplingPhaseThresholdOpen?QStringLiteral("true"):QStringLiteral("false"));
  output.setText(QStringLiteral("SimulationStatus"),
                 QStringLiteral("SIMULATED_ONLY / NOT_PHY_COMPLIANCE"));
  QSaveFile file(name);
  const bool okay=file.open(QIODevice::WriteOnly) &&
                  output.save(&file,"PNG") && file.commit();
  if(!okay)setStatus(S::Error,QStringLiteral("Eye PNG write/commit failed"));
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
      if (workspace && !workspace->remove()) {
        qWarning() << "SParamView: retired analysis workspace cleanup deferred";
        owner_->statusBar()->showMessage("Previous cache could not be removed; cleanup will be retried on exit.");
      }
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
    if (!isReleaseFeature(feature)) continue;
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
    if (!workspace->remove()) {
      qWarning() << "SParamView: retired workspace cleanup deferred";
      owner_->statusBar()->showMessage("Previous cache could not be removed; cleanup will be retried on exit.");
    }
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
void AnalysisWindowManager::setIcnSourceProvider(IcnSourceProvider provider) {
  icnProvider_=std::move(provider);
  if(windows_[int(Feature::Icn)])
    windows_[int(Feature::Icn)]->setIcnSourceProvider(icnProvider_);
  setEngineAvailable(Feature::Icn,bool(icnProvider_));
}
void AnalysisWindowManager::setEyeSourceProvider(EyeSourceProvider provider) {
  eyeProvider_=std::move(provider);
  if(windows_[int(Feature::Eye)])
    windows_[int(Feature::Eye)]->setEyeSourceProvider(eyeProvider_);
  setEngineAvailable(Feature::Eye,bool(eyeProvider_));
}
void AnalysisWindowManager::setEngineAvailable(Feature feature, bool available) {
  if (!isReleaseFeature(feature)) return;
  available_[int(feature)] = available;
  if (auto *action = actions_[int(feature)])
    action->setEnabled(available);
}
AnalysisWindow *AnalysisWindowManager::openWindow(Feature feature) {
  if (!isReleaseFeature(feature)) return nullptr;
  const int index = int(feature);
  if (windows_[index]) {
    windows_[index]->show();
    windows_[index]->raise();
    windows_[index]->activateWindow();
    return windows_[index];
  }
  auto *window = new AnalysisWindow(feature, &scheduler_, owner_);
  if(feature == Feature::Ild) window->setIldSourceProvider(ildProvider_);
  if(feature == Feature::Icn) window->setIcnSourceProvider(icnProvider_);
  if(feature == Feature::Eye) window->setEyeSourceProvider(eyeProvider_);
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
