#include "startup_failure.hpp"
#include <QApplication>
#include <QGuiApplication>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void expect(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    char program[] = "SParamView";
    char selftest[] = "--selftest";
    char validate[] = "--validate-project";
    char file[] = "example.s2p";
    char *ordinaryArgs[] = {program, file};
    char *selftestArgs[] = {program, selftest};
    char *validationArgs[] = {program, validate};
    expect(!sparamview::isAutomatedLaunch(2, ordinaryArgs),
           "Normal interactive launch incorrectly classified as automated");
    expect(sparamview::isAutomatedLaunch(2, selftestArgs),
           "Selftest launch incorrectly classified as interactive");
    expect(sparamview::isAutomatedLaunch(2, validationArgs),
           "Project validation launch incorrectly classified as interactive");
    expect(!sparamview::canShowStartupDialog(true),
           "Automated failure must never open a modal dialog");
    const auto platform = QGuiApplication::platformName().toLower();
    if (platform == "offscreen" || platform == "minimal") {
      expect(!sparamview::canShowStartupDialog(false),
             "Headless GUI platform must not open a modal dialog");
      expect(sparamview::reportStartupFailure("Simulated cache startup failure",
                                               false) == EXIT_FAILURE,
             "Headless startup failure must return a failure code");
    }
    expect(sparamview::reportStartupFailure("Simulated automated startup error",
                                             true) == EXIT_FAILURE,
           "Automated startup failure must return a failure code");
    std::cout << "Stage 3 startup failure tests passed\n";
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    std::cerr << "Stage 3 startup failure tests: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
