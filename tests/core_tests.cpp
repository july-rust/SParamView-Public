#include "si/core.hpp"
#include "temp_directory.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
using namespace si;
static int count = 0;
static void expect(bool condition, const std::string &s) {
  ++count;
  if (!condition)
    throw Error("TEST FAILED: " + s);
}
static void near(double a, double b, double tolerance, const std::string &s) {
  expect(std::abs(a - b) < tolerance,
         s + " got " + std::to_string(a) + " expected " + std::to_string(b));
}
int main() {
  auto dir = fs::temp_directory_path() / "si_core_tests";
  fs::remove_all(dir);
  fs::create_directories(dir);
  TestDirectoryCleanup cleanup{dir};
  try {
    auto write = [&](const std::string &n, const std::string &data) {
      auto p = dir / n;
      std::ofstream(p) << data;
      return p;
    };
    auto p = write("ordering.s2p", "# GHz S RI R 50\n1 .1 0 .2 .1 .3 -.1 .4 "
                                   "0\n2 .2 0 .4 .1 .6 -.1 .8 0\n");
    auto c = Cache::open(p, dir / "cache");
    near(c->trace({{1, 1}}, {{0, 1}})[0].real(), .2, 1e-14, "S21 ordering");
    near(c->trace({{0, 1}}, {{1, 1}})[0].real(), .3, 1e-14, "S12 ordering");
    expect(Cache::open(p, dir / "cache")->reused(), "cache reuse");
    expect(sha256File(p) == c->meta().sha256, "streaming full hash");
    auto sha = write("sha.txt", "abc");
    expect(
        sha256File(sha) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 known answer");
    auto v2 =
        write("row.s2p",
              "[Version] 2.1\n# MHz S DB R 50\n[Number of Ports] 2\n[Two-Port "
              "Data Order] 12_21\n[Number of Frequencies] 1\n[Network "
              "Data]\n1000 -20 0 -10 0 -6.020599913279624 0 -40 0\n[End]\n");
    auto c2 = Cache::open(v2, dir / "cache");
    near(c2->trace({{1, 1}}, {{0, 1}})[0].real(), .5, 1e-12,
         "2.1 row order DB");
    near(c2->frequencies()[0], 1e9, .1, "MHz scaling");
    for (auto matrix : {"Lower", "Upper"}) {
      auto f = write(
          std::string(matrix) + ".s3p",
          std::string("[Version] 2.0\n# Hz S RI R 50\n[Number of Ports] "
                      "3\n[Number of Frequencies] 1\n[Matrix Format] ") +
              matrix + "\n[Network Data]\n1 1 0 2 0 3 0 4 0 5 0 6 0\n[End]\n");
      auto cache = Cache::open(f, dir / "cache");
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          near(cache->trace({{i, 1}}, {{j, 1}})[0].real(),
               cache->trace({{j, 1}}, {{i, 1}})[0].real(), 1e-12,
               "triangle symmetry");
    }
    auto refs =
        write("refs.s2p", "# GHz S RI R 40 75\n1 .1 0 .2 0 .3 0 .4 0\n");
    auto cr = Cache::open(refs, dir / "cache");
    expect(cr->meta().version == "1.1", "implicit version 1.1");
    near(cr->meta().reference[1], 75, 1e-10, "per port reference");
    auto hfss = Cache::open(
        write("hfss-constant.s2p",
              "# Hz S RI R 50\n1 .1 0 .8 0 .7 0 .2 0\n! Port Impedance 50 0\n! "
              "50 0\n2 .1 0 .8 0 .7 0 .2 0\n! Port Impedance 50 0 50 0\n"),
        dir / "cache");
    expect(hfss->meta().warnings.size() == 1,
           "Constant HFSS impedance checked");
    near(hfss->trace({{1, 1}}, {{0, 1}})[0].real(), .8, 1e-14,
         "HFSS comments preserve S");
    for (auto suffix : {std::string("! Port Impedance 50 0 60 0\n"),
                        std::string("! Port Impedance 50 0 50 1\n"),
                        std::string("! Port Impedance 50 0\n")}) {
      bool rejected = false;
      try {
        Cache::open(write("hfss-invalid.s2p",
                          "# Hz S RI R 50\n1 .1 0 .8 0 .7 0 .2 0\n" + suffix),
                    dir / "cache");
      } catch (const Error &) {
        rejected = true;
      }
      expect(rejected, "Inconsistent or truncated HFSS reference rejected");
    }
    Metadata hintMeta;
    hintMeta.ports = 4;
    hintMeta.reference = {50, 50, 50, 50};
    hintMeta.labels = {"U1-A X_N", "U1-B X_P", "J1-1 X_N", "J2-1 X_P"};
    hintMeta.differentialHints = {
        "\"Diff_Channel_X_N_$_X_P\" \"X_N\" \"X_P\" \"Port1_U1_A::X_N\" "
        "\"Port2_U1_B::X_P\" \"Port3_J1_1::X_N\" \"Port4_J2_1::X_P\" \"0\" "
        "\"1\" \"2\" \"3\""};
    auto hints = suggestMapping(hintMeta);
    expect(hints.size() == 1 && hints[0].nearP == 1 && hints[0].nearN == 0 &&
               hints[0].farP == 3 && hints[0].farN == 2,
           "Sigrity explicit negative-first hint normalized");
    std::string hintFile =
        "! Port 1 = U1-A X_N\n! Port 2 = U1-B X_P\n! Port 3 = J1-1 X_N\n! Port "
        "4 = J2-1 X_P\n!.DiffChannels\n!" +
        hintMeta.differentialHints[0] +
        "\n!.EndDiffChannels\n# Hz S RI R 50\n1 ";
    for (int i = 0; i < 16; ++i)
      hintFile += ".1 0 ";
    std::string windowsHint;
    for (char ch : hintFile) {
      if (ch == '\n')
        windowsHint += '\r';
      windowsHint += ch;
    }
    auto hintPath = write("sigrity.s4p", windowsHint + "\r\n");
    auto hintCache = Cache::open(hintPath, dir / "cache");
    expect(suggestMapping(hintCache->meta())[0].nearP == 1,
           "Sigrity hints imported");
    expect(
        suggestMapping(Cache::open(hintPath, dir / "cache")->meta())[0].farN ==
            2,
        "Sigrity hints cached");
    hintMeta.differentialHints.clear();
    auto seHints = suggestMapping(hintMeta);
    expect(seHints.size() == 1 && seHints[0].differential() &&
               seHints[0].nearP == 1 && seHints[0].nearN == 0 &&
               seHints[0].farP == 3 && seHints[0].farN == 2,
           "Terminal P/N net names form an unconfirmed differential-shaped candidate");
    bool threw = false;
    try {
      Cache::open(write("broken.s2p", "# Hz S RI R 50\n1 .1 0 .2 0\n"),
                  dir / "cache");
    } catch (const Error &) {
      threw = true;
    }
    expect(threw, "truncated rejected");
    for (auto &e : fs::directory_iterator(dir / "cache"))
      expect(e.path().extension() != ".tmp", "partial cache removed");
    Trace t;
    t.parameter = "S11";
    t.x = {0, 1, 2, 3, 4, 5};
    for (double v : {-30., -20., -8., -15., -12., -7.})
      t.s.push_back(std::polar(std::pow(10., v / 20.), 0.));
    Settings s;
    s.rl = {true, -10, {}};
    auto r = analyze(t, Metric::RL, s);
    near(r.worst, -7, 1e-10, "RL maximum");
    near(r.margin, -3, 1e-10, "RL margin");
    expect(r.status == "NG", "RL NG");
    expect(r.peaks[0].global && r.peaks[0].x == 5, "band edge global");
    s.rl.enabled = false;
    expect(analyze(t, Metric::RL, s).status == "N/A", "no limit N/A");
    s.il = {true, -25, {}};
    auto il = analyze(t, Metric::IL, s);
    near(il.worst, -30, 1e-10, "IL minimum");
    near(il.margin, -5, 1e-10, "IL margin");
    s.rl = {true, 0, {{0, -40}, {5, -1}}};
    r = analyze(t, Metric::RL, s);
    expect(r.marginX != r.worstX, "margin worst independent of metric worst");
    s.rl = {true, 0, {{1, -20}, {4, -20}}};
    expect(analyze(t, Metric::RL, s).status == "NG", "covered violation keeps NG despite uncovered limit region");
    Trace a;
    a.parameter = "S11";
    a.x = {0, 2};
    a.s = {{1, 0}, {0, 1}};
    auto z = interpolate(a.x, a.s, 1);
    near(z.real(), .5, 1e-14, "complex interpolate real");
    near(logMagnitude(z), -3.01029995664, 1e-9, "interpolate before dB");
    Trace b = a;
    b.x = {0, 1, 2};
    b.s = {{.5, 0}, {.25, .25}, {0, .5}};
    auto d = compare(a, b, Metric::RL);
    near(d.improvement[1], 6.02059991328, 1e-9, "RL improvement sign");
    near(compare(a, b, Metric::IL).improvement[1], -6.02059991328, 1e-9,
         "IL improvement sign");
    std::vector<double> xx(10001), yy(10001);
    std::iota(xx.begin(), xx.end(), 0.);
    yy[4321] = 20;
    yy[5678] = -40;
    auto env = envelope(xx, yy, 100, 0, 10000);
    expect(std::find(env.begin(), env.end(), 4321) != env.end(),
           "narrow peak preserved");
    expect(std::find(env.begin(), env.end(), 5678) != env.end(),
           "narrow notch preserved");
    Trace ideal;
    ideal.parameter = "S11";
    for (int i = 0; i <= 1024; ++i) {
      ideal.x.push_back(i * 1e7);
      ideal.s.push_back(0);
    }
    s = Settings{};
    auto td = transformTdr(ideal, s);
    expect(td.quality == "GOOD", "ideal quality");
    for (auto z : td.impedance)
      near(z, 50, 1e-12, "ideal 50 ohm");
    ideal.parameter = "Sdd11";
    ideal.referenceOhm = 100;
    near(transformTdr(ideal, s).impedance[100], 100, 1e-12,
         "ideal differential");
    ideal.referenceOhm = 50;
    ideal.parameter = "S11";
    for (auto &z : ideal.s)
      z = 0.2;
    td = transformTdr(ideal, s);
    near(td.impedance[300], 75, .1, "known 75 ohm step");
    s.targetOhm = 100;
    s.tdrLimitEnabled = true;
    auto tr = analyze(ideal, Metric::TDR, s);
    expect(tr.status == "NG", "target does not renormalize impedance");
    near(tr.referenceOhm, 50, 1e-12, "source reference retained");
    Channel ch;
    ch.name = "DATA0";
    ch.id = "stable";
    ch.nearP = 0;
    ch.farP = 1;
    writeMappingCsv(dir / "map.csv", {ch});
    expect(readMappingCsv(dir / "map.csv")[0].id == "stable",
           "mapping roundtrip");
    auto ch2 = ch;
    ch2.nearP = 9;
    ch2.farP = 15;
    expect(matchChannels({ch}, {ch2})[0] == 0, "matching identity not port");
    expect(overall({"OK", "N/A"}) == "N/A", "overall unknown");
    expect(overall({"N/A", "NG"}) == "NG", "overall NG priority");
    // Deliberately asymmetric near/far coupling: victim near=0/far=1; aggressor
    // near=2/far=3.
    std::ostringstream net;
    net << "# Hz S RI R 50\n1 ";
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        net << (i == 0 && j == 2 ? .01 : i == 1 && j == 2 ? .1 : 0) << " 0 ";
    net << '\n';
    auto multi = Cache::open(write("xtalk.s4p", net.str()), dir / "cache");
    Channel ag = ch;
    ag.id = ag.name = "Aggressor";
    ag.nearP = 2;
    ag.farP = 3;
    s = Settings{};
    near(logMagnitude(loadTrace(*multi, ch, Metric::NEXT, s, &ag).s[0]), -40,
         1e-10, "NEXT response near/stimulus near");
    near(logMagnitude(loadTrace(*multi, ch, Metric::FEXT, s, &ag).s[0]), -20,
         1e-10, "FEXT response far/stimulus near");
    double q = 1 / std::sqrt(2.);
    near(multi->trace({{0, q}, {1, -q}}, {{2, q}, {3, -q}})[0].real(), -.045,
         1e-12, "differential four-term transform");
    Control cancel;
    cancel.cancelled = true;
    threw = false;
    try {
      Cache::open(p, dir / "cancel", &cancel);
    } catch (const Cancelled &) {
      threw = true;
    }
    expect(threw, "import cancellation");
    s = Settings{};
    s.reverse = true;
    near(std::abs(loadTrace(*multi, ch, Metric::NEXT, s, &ag).s[0]), 0, 1e-12,
         "Reverse NEXT uses aggressor far");
    near(std::abs(loadTrace(*multi, ch, Metric::FEXT, s, &ag).s[0]), 0, 1e-12,
         "Reverse FEXT uses victim near");
    auto positive = multi->trace({{0, q}, {1, -q}}, {{2, q}, {3, -q}})[0];
    auto inverted = multi->trace({{1, q}, {0, -q}}, {{2, q}, {3, -q}})[0];
    near(std::abs(positive + inverted), 0, 1e-12, "P/N reversal phase sign");
    Metadata labels;
    labels.ports = 4;
    labels.labels = {"DATA0_P", "DATA0_N", "DATA1_P", "DATA1_N"};
    labels.reference = {50, 50, 50, 50};
    auto proposed = suggestMapping(labels);
    expect(proposed.size() == 2 && proposed[0].nearN == 1 &&
               proposed[0].farP == -1,
           "P/N-only label candidates do not invent far ports");
    Trace flat;
    flat.parameter = "S11";
    flat.x = xx;
    flat.s.assign(xx.size(), Complex(.1, 0));
    s = Settings{};
    s.rl = {true, -15, {}};
    auto flatResult = analyze(flat, Metric::RL, s);
    expect(flatResult.peaks.size() == 1 && flatResult.peaks[0].global,
           "Flat response retains exactly global worst");
    flat.s[4321] = .6;
    auto narrowResult = analyze(flat, Metric::RL, s);
    expect(narrowResult.worstX == 4321, "Narrow resonance global worst");
    expect(*std::max_element(narrowResult.heatRaw.begin(),
                             narrowResult.heatRaw.end()) == narrowResult.worst,
           "Heatmap retains narrow global worst");
    expect(*std::min_element(narrowResult.heatMargin.begin(),
                             narrowResult.heatMargin.end()) < 0,
           "Heatmap retains narrow NG margin");
    auto mismatched = a;
    mismatched.responseReferenceOhm = 75;
    threw = false;
    try {
      compare(a, mismatched, Metric::IL);
    } catch (const Error &) {
      threw = true;
    }
    expect(threw, "Revision output reference mismatch rejected");
    auto cancelSource = dir / "cancel_mid_import.s2p";
    {
      std::ofstream out(cancelSource);
      out << "# Hz S RI R 50\n";
      for (int i = 1; i < 300000; ++i)
        out << i << " .1 0 .8 0 .8 0 .1 0\n";
    }
    Control halfway;
    halfway.progress = [&](double fraction, const std::string &) {
      if (fraction > 0 && fraction < 1)
        halfway.cancelled = true;
    };
    threw = false;
    try {
      Cache::open(cancelSource, dir / "cancel-mid-cache", &halfway);
    } catch (const Cancelled &) {
      threw = true;
    }
    expect(threw, "Cancellation during active streaming import");
    expect(fs::is_empty(dir / "cancel-mid-cache"),
           "Cancelled partial cache is removed");
    Trace narrowPole;
    narrowPole.parameter = "S11";
    for (int i = 0; i < 30; ++i)
      narrowPole.x.push_back(1000 * std::pow(10.0, i / 10.0));
    for (int i = 1; i <= 100; ++i)
      narrowPole.x.push_back(i * 1e8);
    for (double frequency : narrowPole.x)
      narrowPole.s.push_back(
          1.0 / Complex(1, 2 * 3.141592653589793 * frequency * 1e-5));
    auto unresolved = transformTdr(narrowPole, Settings{});
    expect(unresolved.quality == "UNSUITABLE" && unresolved.impedance.empty(),
           "Unresolved low-frequency pole cannot produce a misleading TDR");
    expect(unresolved.note.find("quadrature did not converge") != std::string::npos,
           "Sparse low-frequency data retains an unsuitable verdict");
    Trace resolvedPole;
    resolvedPole.parameter = "S11";
    for (double f = 1; f < 5e7; f *= 1.03) resolvedPole.x.push_back(f);
    for (int i = 2; i <= 800; ++i) resolvedPole.x.push_back(i * 25e6);
    for (double f : resolvedPole.x)
      resolvedPole.s.push_back(1.0 / Complex(1, 2 * 3.141592653589793 * f * 1e-5));
    Settings directSettings; directSettings.tdrStopSeconds = 3e-9;
    auto resolved = transformTdr(resolvedPole, directSettings);
    expect(resolved.quality == "LIMITED" && !resolved.time.empty(),
           "Dense nonuniform RC pole has a bounded direct step transform");
    for (size_t i = 0; i < resolved.time.size(); ++i) {
      double rho = 1 - std::exp(-resolved.time[i] / 1e-5);
      near(resolved.impedance[i], 50 * (1 + rho) / (1 - rho), .002,
           "Nonuniform RC low-pass agrees with analytic causal step");
    }
    Control stoppedTdr; stoppedTdr.cancelled = true;
    threw = false;
    try { transformTdr(resolvedPole, directSettings, &stoppedTdr); }
    catch (const Cancelled &) { threw = true; }
    expect(threw, "Nonuniform transform honors cancellation");
    Metadata mixed;
    mixed.ports = 6; mixed.reference.assign(6,50);
    mixed.labels = {"U1-1 OUT_P","U1-2 OUT_N","J1-1 OUT_P","J2-1 OUT_N","U1-3 TEST","J3-1 TEST"};
    mixed.differentialHints = {R"("Diff_Channel_OUT_P_$_OUT_N" "OUT_P" "OUT_N" "Port1_U1_1::OUT_P" "Port2_U1_2::OUT_N" "Port3_J1_1::OUT_P" "Port4_J2_1::OUT_N" "0" "1" "2" "3")"};
    auto mixedMap = suggestMapping(mixed);
    expect(mixedMap.size()==2 && mixedMap[0].differential() && !mixedMap[1].differential(),
           "Mixed explicit differential hints retain unused SE nets");
    expect(mixedMap[1].nearP==4 && mixedMap[1].farP==5 && mixedMap[0].group!=mixedMap[1].group,
           "Mixed mapping covers SE endpoints and separates cross-mode groups");
    std::cout << "PASS: " << count << " assertions\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
