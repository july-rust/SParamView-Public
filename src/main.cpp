#include "app.hpp"
#include <iostream>
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("SParamView");
  QApplication::setOrganizationName("SParamView");
  QApplication::setApplicationVersion(si::version);
  applyLightTheme(app);
  QApplication::setWindowIcon(QIcon(":/SParamView.ico"));
  QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath() +
                                    "/fonts/NanumGothic-Regular.ttf");
  QFont::insertSubstitution("Segoe UI", "NanumGothic");
  QFont::insertSubstitution("Noto Sans CJK KR", "NanumGothic");
  MainWindow window;
  window.installHeaderProjectButtons();
  window.show();
  auto args = app.arguments();
  if (args.size() >= 4 &&
      (args[1] == "--selftest" || args[1] == "--validate-project")) {
    bool success = false;
    QTimer::singleShot(0, [&] {
      try {
        success = args[1] == "--selftest"
                      ? window.selftest(args[2], args[3])
                      : window.validateProject(args[2], args[3]);
      } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
      }
      app.exit(success ? 0 : 1);
    });
  } else if (args.size() > 1)
    QTimer::singleShot(0, [&window, args] { window.openPaths(args.mid(1)); });
  return app.exec();
}
