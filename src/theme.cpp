#include "app.hpp"
#include "gui/window_geometry.hpp"
#include <QFontDatabase>
#include <QFileDialog>
#include <QToolButton>
#include <QPainter>
#include <QPainterPath>

namespace {
// QFileDialog creates fixed-size navigation buttons. The common button
// padding would otherwise consume most of the icon's drawing rectangle.
enum class FileNavGlyph { Back, Forward, Parent, NewFolder, List, Detail };

QIcon fileNavigationIcon(FileNavGlyph glyph) {
  QIcon icon;
  // Generate matching logical-size images at multiple device pixel ratios,
  // avoiding a dependency on tiny platform standard-icon pixmaps.
  for (int ratio : {1,2,3}) {
    for (auto mode : {QIcon::Normal,QIcon::Disabled}) {
      QPixmap pixmap(24*ratio,24*ratio);
      pixmap.setDevicePixelRatio(ratio);
      pixmap.fill(Qt::transparent);
      QPainter painter(&pixmap);
      painter.setRenderHint(QPainter::Antialiasing);
      const QColor ink(mode==QIcon::Disabled?"#8b9aab":"#20374f");
      painter.setPen(QPen(ink,2.0,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
      painter.setBrush(Qt::NoBrush);
      auto line=[&](qreal x1,qreal y1,qreal x2,qreal y2) {
        painter.drawLine(QPointF(x1,y1),QPointF(x2,y2));
      };
      switch(glyph) {
      case FileNavGlyph::Back:
        line(4,12,20,12); line(4,12,11,5); line(4,12,11,19); break;
      case FileNavGlyph::Forward:
        line(4,12,20,12); line(20,12,13,5); line(20,12,13,19); break;
      case FileNavGlyph::Parent:
        line(12,20,12,4); line(12,4,5,11); line(12,4,19,11); break;
      case FileNavGlyph::NewFolder: {
        QPainterPath folder;
        folder.moveTo(3,7);folder.lineTo(3,4);folder.lineTo(10,4);
        folder.lineTo(13,7);folder.lineTo(21,7);folder.lineTo(21,20);
        folder.lineTo(3,20);folder.closeSubpath();
        painter.drawPath(folder);
        line(8,13,16,13);line(12,9,12,17);break;
      }
      case FileNavGlyph::List:
        for(int y:{5,12,19}) {
          painter.drawEllipse(QPointF(4,y),0.8,0.8);line(9,y,21,y);
        }
        break;
      case FileNavGlyph::Detail:
        painter.drawRect(QRectF(3,4,18,16));
        line(3,9,21,9);line(3,14,21,14);line(13,9,13,20);break;
      }
      painter.end();
      icon.addPixmap(pixmap,mode,QIcon::Off);
      icon.addPixmap(pixmap,mode,QIcon::On);
    }
  }
  return icon;
}

class FileDialogNavigationFilter final : public QObject {
public:
  explicit FileDialogNavigationFilter(QObject *parent):QObject(parent) {}
protected:
  bool eventFilter(QObject *object,QEvent *event) override {
    // Scope the override to the standard Qt file picker, including save,
    // multi-open and analysis exports. No navigation/selection changes.
    if(event->type()==QEvent::Show) {
      if(auto *dialog=qobject_cast<QFileDialog *>(object)) {
        static constexpr struct {const char *name; FileNavGlyph glyph;} entries[]={
          {"backButton",FileNavGlyph::Back},{"forwardButton",FileNavGlyph::Forward},
          {"toParentButton",FileNavGlyph::Parent},{"newFolderButton",FileNavGlyph::NewFolder},
          {"listModeButton",FileNavGlyph::List},{"detailModeButton",FileNavGlyph::Detail}};
        for(const auto &entry:entries) {
          if(auto *button=dialog->findChild<QToolButton *>(QString::fromLatin1(entry.name))) {
            button->setFixedSize(34,34);
            button->setIconSize(QSize(24,24));
            button->setIcon(fileNavigationIcon(entry.glyph));
          }
        }
      }
    }
    return QObject::eventFilter(object,event);
  }
};
}
void applyLightTheme(QApplication &app) {
  QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
  app.setStyle("Fusion");
  const auto fontPath=QCoreApplication::applicationDirPath()+"/fonts/NotoSansJP-Variable.ttf";
  const int jpFont=QFontDatabase::addApplicationFont(fontPath);
  if(jpFont>=0) {
    const auto families=QFontDatabase::applicationFontFamilies(jpFont);
    if(!families.isEmpty()) {
      app.setProperty("sparamview.japaneseFontFamily",families.front());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
      for(auto script:{QChar::Script_Hiragana,QChar::Script_Katakana,QChar::Script_Han})
        QFontDatabase::addApplicationFallbackFontFamily(script,families.front());
#endif
      QFont base=app.font();
      auto fallback=base.families();
      if(!fallback.contains(families.front()))fallback<<families.front();
      base.setFamilies(fallback);
      app.setFont(base);
    }
  }
  QPalette palette;
  for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    const bool disabled = group == QPalette::Disabled;
    palette.setColor(group, QPalette::Window, QColor("#f1f5f9"));
    palette.setColor(group, QPalette::WindowText, QColor(disabled ? "#526579" : "#20374f"));
    palette.setColor(group, QPalette::Base, QColor("#ffffff"));
    palette.setColor(group, QPalette::AlternateBase, QColor("#edf2f7"));
    palette.setColor(group, QPalette::Text, QColor(disabled ? "#526579" : "#20374f"));
    palette.setColor(group, QPalette::Button, QColor("#ffffff"));
    palette.setColor(group, QPalette::ButtonText, QColor(disabled ? "#526579" : "#20374f"));
    palette.setColor(group, QPalette::Highlight, QColor("#006c70"));
    palette.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(group, QPalette::ToolTipBase, QColor("#ffffff"));
    palette.setColor(group, QPalette::ToolTipText, QColor("#20374f"));
    palette.setColor(group, QPalette::PlaceholderText, QColor("#526579"));
    palette.setColor(group, QPalette::Link, QColor("#006c70"));
    palette.setColor(group, QPalette::Light, QColor("#ffffff"));
    palette.setColor(group, QPalette::Midlight, QColor("#edf2f7"));
    palette.setColor(group, QPalette::Mid, QColor("#a5b4c3"));
    palette.setColor(group, QPalette::Dark, QColor("#607286"));
    palette.setColor(group, QPalette::Shadow, QColor("#20374f"));
  }
  app.setPalette(palette);
  // Preserve the Windows font policy, but use native system fonts on macOS
  // and Linux rather than forcing the Windows-only Segoe UI family.
#ifdef Q_OS_WIN
  const QString fontRule = QStringLiteral("font-family:'Segoe UI','Malgun Gothic','NanumGothic','Noto Sans JP'; ");
#else
  const QString fontRule;
#endif
  app.setStyleSheet(QString(R"(
QWidget { %1font-size:12px; color:#20374f; }
QMainWindow,QDialog { background:#f1f5f9; }
QMenuBar,QMenu,QStatusBar { background:#ffffff; color:#20374f; }
QMenu::item:selected,QMenuBar::item:selected { background:#006c70; color:#ffffff; }
QMenu::item:disabled { color:#526579; }
QMenu::separator { background:#c2ceda; height:1px; margin:5px; }
QPushButton,QToolButton { background:#ffffff; color:#20374f; border:1px solid #b8c7d7; border-radius:5px; padding:7px 10px; }
QPushButton:hover,QToolButton:hover { background:#e0f1f1; border-color:#006c70; }
QPushButton:pressed,QToolButton:pressed,QToolButton:checked { background:#006c70; color:#ffffff; }
QPushButton:disabled,QToolButton:disabled { background:#edf2f7; color:#526579; }
/* QFileDialog navigation buttons have an independent compact content box. */
QFileDialog QToolButton { padding:3px; }
QFileDialog QToolButton:pressed,QFileDialog QToolButton:checked { background:#d4e9e9; color:#20374f; border-color:#006c70; }
QPushButton#primary { background:#006c70; color:#ffffff; border:none; font-weight:600; padding:12px; }
QLineEdit,QComboBox,QDoubleSpinBox,QSpinBox { background:#ffffff; color:#20374f; border:1px solid #b8c7d7; border-radius:4px; padding:5px; selection-background-color:#006c70; selection-color:#ffffff; }
QAbstractItemView,QComboBox QAbstractItemView { background:#ffffff; alternate-background-color:#edf2f7; color:#20374f; selection-background-color:#006c70; selection-color:#ffffff; border:1px solid #b8c7d7; }
QAbstractItemView::item:selected { background:#006c70; color:#ffffff; }
QAbstractItemView::item:hover:!selected { background:#e0f1f1; color:#20374f; }
QHeaderView::section { background:#edf2f7; color:#20374f; border:none; padding:8px; font-weight:600; }
QTabWidget::pane { border:1px solid #c2ceda; background:#ffffff; }
QTabBar::tab { padding:9px 18px; background:#e5edf4; }
QTabBar::tab:selected { background:#ffffff; color:#006c70; }
QSplitter::handle { background:#c2ceda; border-radius:3px; }
QSplitter::handle:hover { background:#006c70; }
QGroupBox { border:1px solid #c2ceda; border-radius:6px; margin-top:12px; padding-top:8px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 5px; }
QProgressBar { border:0; background:#e2ebf3; height:8px; }
QProgressBar::chunk { background:#006c70; }
QToolTip { background:#ffffff; color:#20374f; border:1px solid #607286; padding:5px; }
)" ).arg(fontRule));
  // Use the existing Qt file picker consistently so the same window bounds,
  // navigation icons and translations apply across supported desktops.
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
  if(!app.property("sparamview.windowGeometryFilterInstalled").toBool()) {
    app.installEventFilter(new sparamview::geometry::WindowGeometryFilter(&app));
    app.setProperty("sparamview.windowGeometryFilterInstalled",true);
  }
  if(!app.property("sparamview.fileDialogNavigationFilterInstalled").toBool()) {
    app.installEventFilter(new FileDialogNavigationFilter(&app));
    app.setProperty("sparamview.fileDialogNavigationFilterInstalled",true);
  }
}
