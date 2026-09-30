#include "app.hpp"
void applyLightTheme(QApplication &app) {
  QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
  app.setStyle("Fusion");
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
  app.setStyleSheet(R"(
QWidget { font-family:'Segoe UI','Malgun Gothic','NanumGothic'; font-size:12px; color:#20374f; }
QMainWindow,QDialog { background:#f1f5f9; }
QMenuBar,QMenu,QStatusBar { background:#ffffff; color:#20374f; }
QMenu::item:selected,QMenuBar::item:selected { background:#006c70; color:#ffffff; }
QMenu::item:disabled { color:#526579; }
QMenu::separator { background:#c2ceda; height:1px; margin:5px; }
QPushButton,QToolButton { background:#ffffff; color:#20374f; border:1px solid #b8c7d7; border-radius:5px; padding:7px 10px; }
QPushButton:hover,QToolButton:hover { background:#e0f1f1; border-color:#006c70; }
QPushButton:pressed,QToolButton:pressed,QToolButton:checked { background:#006c70; color:#ffffff; }
QPushButton:disabled,QToolButton:disabled { background:#edf2f7; color:#526579; }
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
)");
}
