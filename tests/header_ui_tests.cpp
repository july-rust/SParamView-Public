#include "app.hpp"
#include <iostream>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  MainWindow window;
  window.installHeaderProjectButtons();

  auto *openProject = window.findChild<QPushButton *>("headerOpenProjectButton");
  auto *saveProject = window.findChild<QPushButton *>("headerSaveProjectButton");
  if (!openProject || !saveProject) {
    std::cerr << "Header project buttons are missing\n";
    return 1;
  }
  if (openProject->text() != "Open Project" || saveProject->text() != "Save Project") {
    std::cerr << "Header project button labels are incorrect\n";
    return 2;
  }

  // Installation must be idempotent because tests and future startup paths may
  // call the helper more than once.
  window.installHeaderProjectButtons();
  if (window.findChildren<QPushButton *>("headerOpenProjectButton").size() != 1 ||
      window.findChildren<QPushButton *>("headerSaveProjectButton").size() != 1) {
    std::cerr << "Header project buttons were duplicated\n";
    return 3;
  }

  auto *root = window.centralWidget() ? window.centralWidget()->layout() : nullptr;
  auto *first = root && root->count() ? root->itemAt(0) : nullptr;
  auto *banner = first ? dynamic_cast<QBoxLayout *>(first->layout()) : nullptr;
  if (!banner) {
    std::cerr << "Header layout is missing\n";
    return 4;
  }

  QStringList buttons;
  for (int i = 0; i < banner->count(); ++i) {
    auto *item = banner->itemAt(i);
    if (auto *b = item ? qobject_cast<QPushButton *>(item->widget()) : nullptr)
      buttons << b->text();
  }
  const int touchstone = buttons.indexOf("+ Open Touchstone");
  const int open = buttons.indexOf("Open Project");
  const int save = buttons.indexOf("Save Project");
  if (touchstone < 0 || open != touchstone + 1 || save != open + 1) {
    std::cerr << "Header project button order is incorrect\n";
    return 5;
  }

  return 0;
}
