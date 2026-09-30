#include "si/core.hpp"
#include "temp_directory.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace si;
static int checks = 0;
static void expect(bool ok, const char *message) {
  ++checks;
  if (!ok)
    throw Error(message);
}
int main() {
  auto dir = fs::temp_directory_path() / "si_mapping_tests";
  fs::create_directories(dir);
  TestDirectoryCleanup cleanup{dir};
  try {
    // Independent nonreciprocal fixture: victim P/N near=6/1, far=3/5;
    // aggressor P/N near=0/7, far=4/2. All indices are zero-based.
    Complex physical[8][8];
    for (int i = 0; i < 8; ++i)
      for (int j = 0; j < 8; ++j)
        physical[i][j] = Complex(.001 * (1 + i * 8 + j), -.001);
    auto block = [&](int rp, int rn, int sp, int sn, Complex pp, Complex pn,
                     Complex np, Complex nn) {
      physical[rp][sp] = pp;
      physical[rp][sn] = pn;
      physical[rn][sp] = np;
      physical[rn][sn] = nn;
    };
    block(6, 1, 6, 1, {.31, .04}, {.07, .02}, {.09, .01}, {.25, .05});
    block(3, 5, 3, 5, {.21, .08}, {.02, .01}, {.04, .03}, {.09, .02});
    block(3, 5, 6, 1, {.80, .20}, {.02, .01}, {.04, .03}, {.66, .04});
    block(6, 1, 3, 5, {.55, .06}, {.05, .02}, {.03, .02}, {.33, .04});
    block(6, 1, 0, 7, {.04, .01}, {.01, .03}, {.02, .02}, {.05, .02});
    block(3, 5, 0, 7, {.08, .02}, {.02, .06}, {.04, .04}, {.10, .04});
    block(3, 5, 4, 2, {.10, .01}, {.01, .05}, {.03, .04}, {.08, .02});
    block(6, 1, 4, 2, {.12, .02}, {.03, .01}, {.02, .01}, {.09, .04});
    auto source = dir / "scrambled.s8p";
    {
      std::ofstream out(source);
      out << "# Hz S RI R 50\n";
      for (int f : {1, 2}) {
        out << f << ' ';
        for (auto &row : physical)
          for (auto value : row)
            out << value.real() << ' ' << value.imag() << ' ';
        out << '\n';
      }
    }
    auto cache = Cache::open(source, dir / "cache");
    Channel dv, da;
    dv.id = dv.name = "Victim";
    dv.nearP = 6;
    dv.nearN = 1;
    dv.farP = 3;
    dv.farN = 5;
    da.id = da.name = "Aggressor";
    da.nearP = 0;
    da.nearN = 7;
    da.farP = 4;
    da.farN = 2;
    struct Case {
      Metric metric;
      bool reverse;
      Complex diff, single;
      const char *direction;
    };
    const Case cases[] = {
        {Metric::RL, false, {.20, .03}, {.31, .04}, "Near -> Near"},
        {Metric::RL, true, {.12, .03}, {.21, .08}, "Far -> Far"},
        {Metric::IL, false, {.70, .10}, {.80, .20}, "Near -> Far"},
        {Metric::IL, true, {.40, .03}, {.55, .06}, "Far -> Near"},
        {Metric::NEXT,
         false,
         {.03, -.01},
         {.04, .01},
         "Aggressor Near -> Victim Near"},
        {Metric::NEXT,
         true,
         {.07, -.03},
         {.10, .01},
         "Aggressor Far -> Victim Far"},
        {Metric::FEXT,
         false,
         {.06, -.02},
         {.08, .02},
         "Aggressor Near -> Victim Far"},
        {Metric::FEXT,
         true,
         {.08, .02},
         {.12, .02},
         "Aggressor Far -> Victim Near"}};
    for (const auto &test : cases)
      for (bool diff : {false, true}) {
        Settings settings;
        settings.reverse = test.reverse;
        auto victim = dv, aggressor = da;
        if (!diff)
          victim.nearN = victim.farN = aggressor.nearN = aggressor.farN = -1;
        auto trace =
            loadTrace(*cache, victim, test.metric, settings, &aggressor);
        expect(std::abs(trace.s[0] - (diff ? test.diff : test.single)) < 1e-13,
               "Logical mapping complex amplitude");
        expect(trace.referenceOhm == (diff ? 100 : 50),
               "Logical source reference");
        expect(analyze(trace, test.metric, settings).direction ==
                   test.direction,
               "Analysis direction label");
        auto result = runJobs(
            {{cache, victim, aggressor, test.metric, "fixture"}}, settings);
        expect(result[0].direction == test.direction,
               "Job/report direction label");
        if (diff &&
            (test.metric == Metric::NEXT || test.metric == Metric::FEXT)) {
          std::swap(aggressor.nearP, aggressor.nearN);
          std::swap(aggressor.farP, aggressor.farN);
          expect(std::abs(loadTrace(*cache, victim, test.metric, settings,
                                    &aggressor)
                              .s[0] +
                          test.diff) < 1e-13,
                 "Aggressor P/N phase inversion");
          std::swap(victim.nearP, victim.nearN);
          std::swap(victim.farP, victim.farN);
          expect(std::abs(loadTrace(*cache, victim, test.metric, settings,
                                    &aggressor)
                              .s[0] -
                          test.diff) < 1e-13,
                 "Both ends P/N phase inversion cancels");
        }
      }
    expect(workerLimit(0, 100, 64) == 1 && workerLimit(1, 100, 64) == 2 &&
               workerLimit(2, 100, 64) == 8,
           "Performance concurrency budgets");
    expect(workerLimit(1, 100, 1) == 1 && workerLimit(2, 100, 0) == 1 &&
               workerLimit(2, 3, 64) == 3 && workerLimit(2, 0, 64) == 0,
           "Hardware/workload cap");
    for (auto metric : {Metric::RL, Metric::IL, Metric::NEXT, Metric::FEXT}) {
      Settings settings;
      settings.rl = settings.il = settings.next =
          settings.fext = {true, -10, {}};
      Trace trace;
      trace.parameter = "S11";
      trace.x = {1, 2};
      for (double value : {-11., -9.}) {
        trace.s.assign(2, std::pow(10., value / 20.));
        expect(analyze(trace, metric, settings).status ==
                   ((metric == Metric::IL ? value >= -10 : value <= -10)
                        ? "OK"
                        : "NG"),
               "Signed dB limit direction");
      }
      settings.rl = settings.il = settings.next = settings.fext = {true, 0, {}};
      trace.s.assign(2, 1.);
      expect(analyze(trace, metric, settings).status == "OK",
             "Equality satisfies limit");
    }
    {
      Metadata power;
      power.ports = 4;
      power.reference = {50, 50, 50, 50};
      power.labels = {"U1-A CLK_P_OUT_0_MP", "U1-B clk_n_out_0_mp",
                      "J1-1 CLK_P_OUT_0_MP", "J1-2 CLK_N_OUT_0_MP"};
      power.differentialHints = {
          R"hint("D" "CLK_N_OUT_0_MP" "CLK_P_OUT_0_MP" "Port2_U1_B::CLK_N_OUT_0_MP" "Port1_U1_A::CLK_P_OUT_0_MP" "Port4_J1_2::CLK_N_OUT_0_MP" "Port3_J1_1::CLK_P_OUT_0_MP" "1" "0" "3" "2")hint"};
      std::string diagnostic;
      auto mapped = suggestMapping(power, &diagnostic);
      expect(diagnostic.empty() && mapped.size() == 1,
             "PowerSI middle-token differential hint");
      expect(mapped[0].nearP == 0 && mapped[0].nearN == 1 &&
                 mapped[0].farP == 2 && mapped[0].farN == 3,
             "PowerSI reversed P/N hint normalization");
      expect(mapped[0].name == "CLK_OUT_0_MP",
             "PowerSI middle-token channel name");

      power.labels = {"U1-A CLK_POS", "U1-B CLK_NEG",
                      "J1-1 CLK_POS", "J1-2 CLK_NEG"};
      power.differentialHints = {
          R"hint("D" "CLK_POS" "CLK_NEG" "a" "b" "c" "d" "0" "1" "2" "3")hint"};
      diagnostic.clear();
      mapped = suggestMapping(power, &diagnostic);
      expect(!diagnostic.empty() && mapped.size() == 2,
             "PowerSI compatibility fallback remains conservative");
      expect(!mapped[0].differential() && !mapped[1].differential(),
             "Unsupported POS/NEG is not guessed as P/N");
    }
    {
      Metadata names;
      names.ports = 4;
      names.reference = {50, 50, 50, 50};
      names.labels = {"U1-A DATA_P", "U1-B DATA_N",
                      "J1-1 DATA_P", "J1-2 DATA_N"};
      std::string diagnostic;
      auto mapped = suggestMapping(names, &diagnostic);
      expect(diagnostic.empty() && mapped.size() == 1 && mapped[0].differential(),
             "P/N terminal net names form a differential candidate");
      expect(mapped[0].nearP == 0 && mapped[0].nearN == 1 &&
                 mapped[0].farP == 2 && mapped[0].farN == 3,
             "Name candidate preserves physical endpoint ordering");

      names.labels = {"U1-A CLK_P_OUT_0_MP", "U1-B CLK_N_OUT_0_MP",
                      "J1-1 CLK_P_OUT_0_MP", "J1-2 CLK_N_OUT_0_MP"};
      mapped = suggestMapping(names, &diagnostic);
      expect(mapped.size() == 1 && mapped[0].differential() &&
                 mapped[0].name == "CLK_OUT_0_MP",
             "Middle P/N token terminal names form one candidate");
    }
    std::cout << "PASS " << checks << " mapping/sign checks\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
