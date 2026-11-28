#pragma once

#include "../user_preferences.hpp"
#include <QApplication>
#include <QDialog>
#include <QFileDialog>
#include <QMainWindow>
#include <QPointer>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>

namespace sparamview::geometry {
inline QScreen *targetScreen(QWidget *widget, bool preferOwner = false) {
  if (preferOwner && widget->parentWidget())
    if (auto *screen = widget->parentWidget()->window()->screen()) return screen;
  if (auto *screen = widget->screen()) return screen;
  return QGuiApplication::primaryScreen();
}

// Correct the actual native frame in logical pixels. Never change explicit
// maximized, minimized or fullscreen state. Preserve a valid user position.
inline void fitWindow(QWidget *widget, QScreen *screen, bool center = false) {
  if (!widget || !screen || !widget->isWindow() || widget->isMaximized() ||
      widget->isFullScreen() || widget->isMinimized()) return;
  const QRect area = screen->availableGeometry();
  if (!area.isValid()) return;
  const auto margins = widget->windowHandle() ? widget->windowHandle()->frameMargins() : QMargins{};
  const QSize cap = preferences::boundedDialogClientSize(area.size(), area, margins);
  const QSize fitted = widget->size().boundedTo(cap);
  if (widget->size() != fitted) widget->resize(fitted);
  const QRect frame = widget->frameGeometry();
  const QSize frameSize(widget->width() + qMax(0,margins.left()) + qMax(0,margins.right()),
                        widget->height() + qMax(0,margins.top()) + qMax(0,margins.bottom()));
  const QSize actual = frame.size().expandedTo(frameSize);
  QPoint top = frame.topLeft();
  if (center) top = preferences::centeredDialogFrame(actual,area);
  else {
    top.setX(std::clamp(top.x(),area.left(),qMax(area.left(),area.right()-actual.width()+1)));
    top.setY(std::clamp(top.y(),area.top(),qMax(area.top(),area.bottom()-actual.height()+1)));
  }
  if (top != frame.topLeft()) widget->move(widget->pos()+top-frame.topLeft());
}

inline void prepareDialog(QDialog &dialog, QSize preferred) {
  if (auto *screen = targetScreen(&dialog,true)) {
    dialog.winId();
    if (dialog.windowHandle() && dialog.windowHandle()->screen() != screen)
      dialog.windowHandle()->setScreen(screen);
    const auto rect = preferences::centeredNormalGeometry(preferred,screen->availableGeometry());
    if (rect.isValid()) { dialog.resize(rect.size()); dialog.move(rect.topLeft()); }
    fitWindow(&dialog,screen,true);
  }
}

// Fixed footer outside a scrolling body: content remains accessible even
// when translated text or a high DPI scale reduces logical screen space.
inline QWidget *scrollBody(QVBoxLayout *outer, const QString &name = QStringLiteral("boundedDialogScroll")) {
  auto *scroll = new QScrollArea;
  scroll->setObjectName(name);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setMinimumSize(0,0);
  auto *content = new QWidget;
  scroll->setWidget(content);
  outer->addWidget(scroll,1);
  return content;
}
inline QVBoxLayout *scrollColumn(QVBoxLayout *outer, const QString &name = QStringLiteral("boundedDialogScroll")) {
  auto *body = new QVBoxLayout(scrollBody(outer,name));
  body->setContentsMargins(0,0,0,0);
  return body;
}

// Application-wide safety net for QWidget top-level windows. Do not move
// transient menus/tooltips or repeatedly recenter a window while it is used.
class WindowGeometryFilter final : public QObject {
public:
  explicit WindowGeometryFilter(QApplication *app):QObject(app) {
    auto watch = [this](QScreen *screen) {
      if (!screen) return;
      connect(screen,&QScreen::availableGeometryChanged,this,[this](const QRect &) {
        for(auto *widget : QApplication::topLevelWidgets())
          if (managed(widget) && widget->isVisible()) schedule(widget,false);
      });
    };
    for(auto *screen : app->screens()) watch(screen);
    connect(app,&QGuiApplication::screenAdded,this,watch);
    connect(app,&QGuiApplication::screenRemoved,this,[this](QScreen *) {
      for(auto *widget : QApplication::topLevelWidgets())
        if(managed(widget) && widget->isVisible()) schedule(widget,false);
    });
  }
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    auto *widget=qobject_cast<QWidget *>(object);
    if (!managed(widget)) return false;
    if(event->type()==QEvent::Show) {
      // Native frame margins may only be reliable after the window is shown.
      schedule(widget,qobject_cast<QDialog *>(widget)!=nullptr);
    } else if(event->type()==QEvent::WindowStateChange ||
              event->type()==QEvent::ScreenChangeInternal) {
      if(widget->isVisible()) schedule(widget,false);
    }
    return false;
  }
private:
  static bool managed(QWidget *widget) {
    return widget && widget->isWindow() &&
      (qobject_cast<QDialog *>(widget) || qobject_cast<QMainWindow *>(widget) ||
       widget->objectName().startsWith(QStringLiteral("channelAnalysisWindow")));
  }
  static void schedule(QWidget *widget,bool center) {
    QPointer<QWidget> guard(widget);
    QTimer::singleShot(0,widget,[guard,center] {
      if(guard && guard->isVisible())
        fitWindow(guard.data(),targetScreen(guard.data(),center),center);
    });
  }
};
} // namespace sparamview::geometry
