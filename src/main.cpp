#include "app.hpp"
#include "startup_failure.hpp"
#include <iostream>

void applyCompactAnalysisSettings(MainWindow &window);

int main(int argc, char **argv) {
  const bool automated = sparamview::isAutomatedLaunch(argc, argv);
  // A failure before QApplication exists cannot safely display Qt UI.
  try {
    QApplication app(argc, argv);
    try {
      QApplication::setApplicationName("SParamView");
      QApplication::setOrganizationName("SParamView");
      QApplication::setApplicationVersion(si::version);
      applyLightTheme(app);
      QApplication::setWindowIcon(QIcon(":/SParamView.ico"));
      QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath() +
                                        "/fonts/NanumGothic-Regular.ttf");
#ifdef Q_OS_WIN
      QFont::insertSubstitution("Segoe UI", "NanumGothic");
#endif
      QFont::insertSubstitution("Noto Sans CJK KR", "NanumGothic");
      MainWindow window;
      applyCompactAnalysisSettings(window);
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
    } catch (const std::exception &e) {
      return sparamview::reportStartupFailure(QString::fromUtf8(e.what()),
                                               automated);
    } catch (...) {
      return sparamview::reportStartupFailure(
          QStringLiteral("Unexpected non-standard startup exception"),
          automated);
    }
  } catch (const std::exception &e) {
    std::cerr << "SParamView initialization failed before GUI was available: "
              << e.what() << '\n';
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "SParamView initialization failed before GUI was available: "
                 "unknown exception\n";
    return EXIT_FAILURE;
  }
}
