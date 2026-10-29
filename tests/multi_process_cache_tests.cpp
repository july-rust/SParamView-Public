// Real multi-process regression: two SParamView cache owners may open the
// same Touchstone inputs without sharing cache files or deleting one another.
#include "cache_session.hpp"
#include "si/core.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static void expect(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

static void writeSource(const QString &path, const QByteArray &data) {
  QFile f(path);
  expect(f.open(QIODevice::WriteOnly) && f.write(data) == data.size(),
         "Cannot write synthetic Touchstone fixture");
}

static int child(const QStringList &args) {
  expect(args.size() == 5, "Invalid child arguments");
  CacheSession session(args[2]);
  std::vector<std::shared_ptr<si::Cache>> caches;
  for (int i = 3; i < 5; ++i) {
    caches.push_back(si::Cache::open(
        si::pathFromUtf8(args[i].toUtf8().toStdString()),
        si::pathFromUtf8(session.path().toUtf8().toStdString())));
  }

  QByteArray ready("READY\t");
  ready += session.path().toUtf8();
  for (const auto &cache : caches)
    ready += "\t" + QByteArray::fromStdString(si::utf8(cache->path()));
  ready += '\n';
  std::cout.write(ready.constData(), ready.size());
  std::cout.flush();

  // Keep the file handles and the lease alive while other processes open
  // identical sources, exit, or scan for abandoned cache directories.
  std::string command;
  if (!std::getline(std::cin, command) || command != "exit")
    return 3;
  caches.clear(); // Release CacheFile handles before removing the directory.
  return session.cleanup() ? 0 : 4;
}

struct ChildPaths {
  QString session;
  QString first;
  QString second;
};

static void start(QProcess &process, const QString &root,
                  const QString &first, const QString &second) {
  process.setProcessChannelMode(QProcess::SeparateChannels);
  process.start(QCoreApplication::applicationFilePath(),
                {"--child", root, first, second});
}

static ChildPaths ready(QProcess &process) {
  expect(process.waitForStarted(15000), "Child did not start");
  QByteArray output = process.readAllStandardOutput();
  while (!output.contains('\n')) {
    if (!process.waitForReadyRead(20000)) {
      output += process.readAllStandardOutput();
      if (output.contains('\n'))
        break;
      const QByteArray childError = process.readAllStandardError();
      throw std::runtime_error(
          std::string("Child did not report cache readiness: state=") +
          std::to_string(int(process.state())) + " exit=" +
          std::to_string(process.exitCode()) + " processError=" +
          process.errorString().toStdString() + " stderr=" +
          childError.toStdString() + " stdout=" + output.toStdString());
    }
    output += process.readAllStandardOutput();
  }
  QByteArray line = output.left(output.indexOf('\n'));
  if (line.endsWith('\r'))
    line.chop(1); // Windows CRT translates child stdout LF to CRLF.
  const auto items = line.split('\t');
  expect(items.size() == 4 && items[0] == "READY", "Invalid child readiness report");
  ChildPaths paths{QString::fromUtf8(items[1]), QString::fromUtf8(items[2]),
                   QString::fromUtf8(items[3])};
  expect(QDir(paths.session).exists() && QFileInfo::exists(paths.first) &&
             QFileInfo::exists(paths.second),
         "Child cache files are not present while session is active");
  return paths;
}

static void stopNormally(QProcess &process) {
  expect(process.state() == QProcess::Running, "Child exited before shutdown");
  expect(process.write("exit\n") == 5, "Could not request child shutdown");
  process.closeWriteChannel();
  expect(process.waitForFinished(20000), "Child did not exit after shutdown");
  expect(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
         "Child could not clean its private cache");
}

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    const auto args = app.arguments();
    if (args.size() > 1 && args[1] == "--child")
      return child(args);

    QTemporaryDir sandbox;
    expect(sandbox.isValid(), "Cannot create multi-process test sandbox");
    const auto root = QDir(sandbox.path()).filePath("cache/v3");
    const auto sourceA = QDir(sandbox.path()).filePath("a.s2p");
    const auto sourceB = QDir(sandbox.path()).filePath("b.s2p");
    const auto savedProject = QDir(sandbox.path()).filePath("saved.siproject");
    const QByteArray touchstone =
        "# Hz S RI R 50\n1 .1 0 .8 0 .6 0 .2 0\n2 .1 0 .8 0 .6 0 .2 0\n";
    writeSource(sourceA, touchstone);
    writeSource(sourceB, touchstone);
    writeSource(savedProject, "preserve project");

    QProcess first, second, third;
    // Start concurrently: both processes register ownership in the same root.
    start(first, root, sourceA, sourceB);
    start(second, root, sourceA, sourceB);
    const auto a = ready(first), b = ready(second);
    expect(a.session != b.session && a.first != b.first &&
               a.second != b.second,
           "Concurrent processes reused a session or cache file");

    // Closing A may delete only its own two imported caches, never B's.
    stopNormally(first);
    expect(!QDir(a.session).exists() && !QFileInfo::exists(a.first) &&
               !QFileInfo::exists(a.second),
           "Normal exit left the first process cache behind");
    expect(QDir(b.session).exists() && QFileInfo::exists(b.first) &&
               QFileInfo::exists(b.second),
           "First process cleanup damaged the second live process");

    // Simulate a forced termination. C must reclaim B without deleting its
    // own cache or any user source or saved project.
    second.kill();
    expect(second.waitForFinished(20000), "Killed child did not terminate");
    expect(QDir(b.session).exists(), "Crash fixture unexpectedly disappeared");
    start(third, root, sourceA, sourceB);
    const auto c = ready(third);
    expect(!QDir(b.session).exists(), "Stale session not reclaimed");
    expect(QDir(c.session).exists() && QFileInfo::exists(c.first) &&
               QFileInfo::exists(c.second), "Reaper damaged the new session");
    stopNormally(third);
    expect(!QDir(c.session).exists(), "Last process did not remove own cache");
    expect(QFileInfo::exists(sourceA) && QFileInfo::exists(sourceB) &&
               QFileInfo::exists(savedProject),
           "Cache cleanup removed an original source or saved project");

    std::cout << "Multi-process cache isolation and recovery: PASS\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Multi-process cache test: " << e.what() << '\n';
    return 1;
  }
}
