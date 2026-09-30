#include "app.hpp"
#include <algorithm>
PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(360, 180);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::CrossCursor);
  settleTimer.setSingleShot(true);
  settleTimer.setInterval(160);
  connect(&settleTimer, &QTimer::timeout, this, [this] {
    if (rangeChanged) rangeChanged(snapshot.viewStart, snapshot.viewStop);
  });
}
PlotWidget::View PlotWidget::currentView() const {
  if (graphLayerValid()) return cachedView;
  auto [l,r] = graphXRange(snapshot);
  auto [b,t] = graphYRange(snapshot);
  return {l,r,b,t};
}
void PlotWidget::rememberView() {
  history.push_back({snapshot.viewStart, snapshot.viewStop,
                     snapshot.viewBottom, snapshot.viewTop});
  if (history.size() > 40) history.erase(history.begin());
}
void PlotWidget::invalidateGraph() { graphLayer = {}; update(); }
bool PlotWidget::graphLayerValid() const {
  auto equal = [](double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); };
  return !graphLayer.isNull() && cachedSize == size() && cachedDpr == devicePixelRatioF() &&
    equal(cachedAxes.left,snapshot.viewStart) && equal(cachedAxes.right,snapshot.viewStop) &&
    equal(cachedAxes.bottom,snapshot.viewBottom) && equal(cachedAxes.top,snapshot.viewTop);
}
void PlotWidget::notifyRange() { update(); settleTimer.start(); }
void PlotWidget::resetView() {
  settleTimer.stop();
  snapshot.viewStart = snapshot.viewStop = snapshot.viewBottom = snapshot.viewTop = si::NaN;
  history.clear(); dragging = false; selection = {};
  invalidateGraph();
}
void PlotWidget::fitView() {
  rememberView();
  snapshot.viewStart = snapshot.viewStop = snapshot.viewBottom = snapshot.viewTop = si::NaN;
  notifyRange();
}
void PlotWidget::previousView() {
  if (history.empty()) return;
  auto v = history.back(); history.pop_back();
  snapshot.viewStart=v.left; snapshot.viewStop=v.right;
  snapshot.viewBottom=v.bottom; snapshot.viewTop=v.top;
  notifyRange();
}
void PlotWidget::setView(double l, double r, double b, double t, bool remember) {
  if (!std::isfinite(l) || !std::isfinite(r) || r <= l) return;
  if (std::isfinite(b) != std::isfinite(t) || (std::isfinite(b) && t<=b)) return;
  if (snapshot.metric != si::Metric::TDR && l < 0) { r -= l; l=0; }
  if (remember) rememberView();
  snapshot.viewStart=l; snapshot.viewStop=r;
  snapshot.viewBottom=b; snapshot.viewTop=t;
  notifyRange();
}
void PlotWidget::setBoxZoom(bool enabled) { boxZoom=enabled; setCursor(enabled ? Qt::CrossCursor : Qt::OpenHandCursor); }
void PlotWidget::zoomAt(double factor, QPointF pixel, bool x, bool y) {
  if (snapshot.curves.empty()) return;
  if (snapshot.heatmap && snapshot.metric!=si::Metric::TDR) { x=true; y=false; }
  auto box=graphArea(rect(),snapshot); auto v=currentView();
  double fx=std::clamp((pixel.x()-box.left())/box.width(),0.,1.);
  double fy=std::clamp((box.bottom()-pixel.y())/box.height(),0.,1.);
  double cx=v.left+fx*(v.right-v.left), cy=v.bottom+fy*(v.top-v.bottom);
  double l=x ? cx+(v.left-cx)*factor : v.left;
  double r=x ? cx+(v.right-cx)*factor : v.right;
  double b=y ? cy+(v.bottom-cy)*factor : snapshot.viewBottom;
  double t=y ? cy+(v.top-cy)*factor : snapshot.viewTop;
  if (r-l < std::max(1e-18,std::abs(cx)*1e-12)) return;
  setView(l,r,b,t);
}
void PlotWidget::zoomBy(double factor) { zoomAt(factor,graphArea(rect(),snapshot).center(),true,false); }
void PlotWidget::wheelEvent(QWheelEvent *e) {
  if (!graphArea(rect(),snapshot).contains(e->position())) { e->ignore(); return; }
  double delta=e->angleDelta().y();
  if (delta==0) delta=e->pixelDelta().y();
  if (delta==0) return;
  bool control=e->modifiers().testFlag(Qt::ControlModifier);
  bool shift=e->modifiers().testFlag(Qt::ShiftModifier);
  zoomAt(delta>0 ? .8 : 1.25,e->position(),!control,control || shift);
  e->accept();
}
void PlotWidget::mousePressEvent(QMouseEvent *e) {
  if (e->button()!=Qt::LeftButton || snapshot.curves.empty() || !graphArea(rect(),snapshot).contains(e->position())) return;
  settleTimer.stop(); setFocus(); dragging=true; anchor=pointer=e->position();
  dragView=currentView(); selection={};
  dragBox=boxZoom || e->modifiers().testFlag(Qt::ShiftModifier);
  rememberView();
  if (dragBox) selection=QRectF(anchor,anchor);
  else setCursor(Qt::ClosedHandCursor);
  e->accept();
}
void PlotWidget::mouseMoveEvent(QMouseEvent *e) {
  auto box=graphArea(rect(),snapshot);pointer=e->position();pointerInside=box.contains(pointer);
  if (dragging) {
    if (dragBox)
      selection=QRectF(anchor,pointer).normalized().intersected(box);
    else {
      double dx=(pointer.x()-anchor.x())/box.width()*(dragView.right-dragView.left);
      double dy=(pointer.y()-anchor.y())/box.height()*(dragView.top-dragView.bottom);
      snapshot.viewStart=dragView.left-dx;snapshot.viewStop=dragView.right-dx;
      if (snapshot.metric!=si::Metric::TDR && snapshot.viewStart<0) {
        snapshot.viewStop-=snapshot.viewStart; snapshot.viewStart=0;
      }
      if (!snapshot.heatmap || snapshot.metric==si::Metric::TDR) {
        snapshot.viewBottom=dragView.bottom+dy;snapshot.viewTop=dragView.top+dy;
      }
    }
  }
  if (pointerInside && !snapshot.curves.empty()) {
    auto v=currentView(); double x=v.left+(pointer.x()-box.left())/box.width()*(v.right-v.left);
    double scale=snapshot.metric==si::Metric::TDR?1e9:1e-9;
    QString text=num(x*scale,5)+(snapshot.metric==si::Metric::TDR?" ns":" GHz");
    if (snapshot.heatmap && snapshot.metric!=si::Metric::TDR) {
      size_t row=std::min(snapshot.curves.size()-1,size_t(std::max(0.,(pointer.y()-box.top())/box.height())*snapshot.curves.size()));
      const auto &curve=snapshot.curves[row];
      const auto &values=snapshot.margin?curve.heatMargin:curve.heatRaw;
      text+="  |  "+qs(curve.channel);
      if (!values.empty() && x>=curve.start && x<=curve.stop && curve.stop>curve.start) {
        size_t bin=std::min(values.size()-1,size_t((x-curve.start)/(curve.stop-curve.start)*values.size()));
        text+="  "+num(values[bin],3)+" dB  ("+(snapshot.margin?QString("cell worst margin"):QString("cell worst value"))+")";
      }
      if(cursorChanged)cursorChanged(text);
      setToolTip(text);update();return;
    }
    const si::Result *nearest=nullptr; double value=0,distance=1e100;
    for (const auto &c:snapshot.curves) {
      auto it=std::lower_bound(c.plotX.begin(),c.plotX.end(),x);
      if (it==c.plotX.end() || x<c.plotX.front()) continue;
      size_t k=size_t(it-c.plotX.begin()); double y=c.plotY[k];
      if (k>0 && *it!=x) y=c.plotY[k-1]+(c.plotY[k]-c.plotY[k-1])*(x-c.plotX[k-1])/(c.plotX[k]-c.plotX[k-1]);
      if (!std::isfinite(y)) continue;
      double py=box.bottom()-(y-v.bottom)/(v.top-v.bottom)*box.height();
      if (std::abs(py-pointer.y())<distance) { nearest=&c;value=y;distance=std::abs(py-pointer.y()); }
    }
    if (nearest) {
      text += "  |  " + qs(nearest->channel); if(snapshot.bothDirections) text += nearest->reverse ? " F→N" : " N→F"; text += "  " + num(value,3) + (snapshot.metric==si::Metric::TDR?(snapshot.settings.tdrReflection?" ρ":" Ω"):" dB") + "  (displayed trace)";
      if(snapshot.bothDirections&&(snapshot.metric==si::Metric::RL||snapshot.metric==si::Metric::IL)){auto sameLogical=[&](const si::Result &other){return other.reverse!=nearest->reverse&&other.revision==nearest->revision&&other.channelId==nearest->channelId&&other.aggressorId==nearest->aggressorId&&other.termination==nearest->termination;};auto sample=[&](const si::Result &curve)->double{if(curve.plotX.empty()||x<curve.plotX.front()||x>curve.plotX.back())return si::NaN;auto it=std::lower_bound(curve.plotX.begin(),curve.plotX.end(),x);size_t k=size_t(it-curve.plotX.begin());if(k==0||it==curve.plotX.end()||*it==x)return curve.plotY[std::min(k,curve.plotY.size()-1)];return curve.plotY[k-1]+(curve.plotY[k]-curve.plotY[k-1])*(x-curve.plotX[k-1])/(curve.plotX[k]-curve.plotX[k-1]);};auto other=std::find_if(snapshot.curves.begin(),snapshot.curves.end(),sameLogical);if(other!=snapshot.curves.end()){double paired=sample(*other);if(std::isfinite(paired)&&std::isfinite(value))text += "  |  Δ"+QString(snapshot.metric==si::Metric::IL?"IL ":"RL ")+num(std::abs(value-paired),3)+" dB";}}
    }
    if (cursorChanged) cursorChanged(text);
    setToolTip(text+"\nWheel: X zoom · Ctrl+wheel: Y zoom · Drag: pan · Shift+drag: box zoom\nHome: fit all · Backspace: previous view");
  }
  update();
}
void PlotWidget::mouseReleaseEvent(QMouseEvent *e) {
  if (!dragging || e->button()!=Qt::LeftButton) return;
  dragging=false;
  auto box=graphArea(rect(),snapshot);
  if (selection.width()>6 && selection.height()>6) {
    auto v=dragView;
    double l=v.left+(selection.left()-box.left())/box.width()*(v.right-v.left);
    double r=v.left+(selection.right()-box.left())/box.width()*(v.right-v.left);
    double b=v.bottom+(box.bottom()-selection.bottom())/box.height()*(v.top-v.bottom);
    double t=v.bottom+(box.bottom()-selection.top())/box.height()*(v.top-v.bottom);
    const bool heat=snapshot.heatmap && snapshot.metric!=si::Metric::TDR;
    setView(l,r,heat?si::NaN:b,heat?si::NaN:t,false);
  }
  selection={};setCursor(boxZoom?Qt::CrossCursor:Qt::OpenHandCursor);notifyRange();
}
void PlotWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  dragging=false; selection={};
  auto box=graphArea(rect(),snapshot);
  if (!box.contains(e->position())) { fitView();return; }
  if (snapshot.metric==si::Metric::TDR) { zoomAt(.5,e->position(),true,false);return; }
  auto [l,r]=graphXRange(snapshot);
  double x=l+(e->position().x()-box.left())/box.width()*(r-l);
  if (markerAdded && x>=0) markerAdded(x);
}
void PlotWidget::leaveEvent(QEvent *) { pointerInside=false; if(cursorChanged)cursorChanged({});update(); }
void PlotWidget::keyPressEvent(QKeyEvent *e) {
  if (e->key()==Qt::Key_Home) fitView();
  else if (e->key()==Qt::Key_Backspace) previousView();
  else if (e->key()==Qt::Key_Plus || e->key()==Qt::Key_Equal) zoomBy(.8);
  else if (e->key()==Qt::Key_Minus) zoomBy(1.25);
  else if (e->key()==Qt::Key_Escape) { dragging=false;selection={};update(); }
  else if (e->key()==Qt::Key_Left || e->key()==Qt::Key_Right) {
    auto v=currentView();double d=(v.right-v.left)*.15*(e->key()==Qt::Key_Left?-1:1);
    setView(v.left+d,v.right+d,snapshot.viewBottom,snapshot.viewTop);
  } else QWidget::keyPressEvent(e);
}
void PlotWidget::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);
  menu.addAction("Fit All",this,[this]{fitView();});
  menu.addAction("Previous View",this,[this]{previousView();});
  menu.addAction("Zoom In",this,[this]{zoomBy(.8);});
  menu.addAction("Axis Limits...",this,[this]{editAxes();});
  menu.exec(e->globalPos());
}
void PlotWidget::editAxes() {
  if (snapshot.curves.empty()) return;
  auto v=currentView();double scale=snapshot.metric==si::Metric::TDR?1e9:1e-9;
  QDialog dialog(this);dialog.setWindowTitle("Plot Axis Limits");QFormLayout layout(&dialog);
  QDoubleSpinBox l,r,b,t;
  for (auto p:{&l,&r,&b,&t}) { p->setDecimals(9);p->setRange(-1e12,1e12); }
  l.setValue(v.left*scale);r.setValue(v.right*scale);b.setValue(v.bottom);t.setValue(v.top);
  QString x=snapshot.metric==si::Metric::TDR?" [ns]":" [GHz]";
  layout.addRow("X Start"+x,&l);layout.addRow("X Stop"+x,&r);
  layout.addRow("Y Minimum",&b);layout.addRow("Y Maximum",&t);
  QCheckBox automatic("Auto Y Range");automatic.setChecked(!std::isfinite(snapshot.viewBottom));layout.addRow(&automatic);
  if(snapshot.heatmap && snapshot.metric!=si::Metric::TDR) { b.setEnabled(false);t.setEnabled(false);automatic.setChecked(true);automatic.setEnabled(false); }
  QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addRow(&buttons);
  connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(r.value()<=l.value() || (!automatic.isChecked() && t.value()<=b.value())) { QMessageBox::warning(&dialog,"Invalid Axis Range","The upper limit must be greater than the lower limit.");return; }
    dialog.accept();
  });
  connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()==QDialog::Accepted) setView(l.value()/scale,r.value()/scale,automatic.isChecked()?si::NaN:b.value(),automatic.isChecked()?si::NaN:t.value());
}
void PlotWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  invalidateGraph();
  if (snapshot.metric != si::Metric::TDR || snapshot.curves.empty() ||
      !rangeChanged)
    return;
  const bool reduced = std::any_of(
      snapshot.curves.begin(), snapshot.curves.end(),
      [](const auto &curve) { return !curve.plotComplete; });
  if (!reduced)
    return;
  const int newWidth = event->size().width();
  if (tdrDetailWidth != 0 && std::abs(newWidth - tdrDetailWidth) < 192)
    return;
  tdrDetailWidth = newWidth;
  QTimer::singleShot(0, this, [this] {
    if (snapshot.metric == si::Metric::TDR && rangeChanged &&
        std::any_of(snapshot.curves.begin(), snapshot.curves.end(),
                    [](const auto &curve) { return !curve.plotComplete; }))
      rangeChanged(snapshot.viewStart, snapshot.viewStop);
  });
}
void PlotWidget::paintEvent(QPaintEvent *) {
  if (!graphLayerValid()) {
    cachedSize = size(); cachedDpr = devicePixelRatioF();
    cachedAxes = {snapshot.viewStart,snapshot.viewStop,snapshot.viewBottom,snapshot.viewTop};
    auto [l,r] = graphXRange(snapshot); auto [b,t] = graphYRange(snapshot);
    cachedView = {l,r,b,t};
    graphLayer = QPixmap((QSizeF(size()) * cachedDpr).toSize());
    graphLayer.setDevicePixelRatio(cachedDpr);
    QPainter layer(&graphLayer); paintGraph(layer,rect(),snapshot,true);
    ++renderCount;
  }
  QPainter p(this);p.drawPixmap(0,0,graphLayer);
  auto box=graphArea(rect(),snapshot);p.setClipRect(box);
  if (pointerInside && !snapshot.curves.empty()) {
    p.setPen(QPen(QColor("#8396aa"),1,Qt::DotLine));
    p.drawLine(QPointF(pointer.x(),box.top()),QPointF(pointer.x(),box.bottom()));
    if(!snapshot.heatmap || snapshot.metric==si::Metric::TDR)p.drawLine(QPointF(box.left(),pointer.y()),QPointF(box.right(),pointer.y()));
  }
  if (!selection.isEmpty()) { p.setPen(QPen(QColor("#006c70"),1,Qt::DashLine));p.setBrush(QColor(0,108,112,35));p.drawRect(selection); }
}
