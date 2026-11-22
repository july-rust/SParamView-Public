#include "si/channel_analysis/com_voltage_ratio.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace si::channel {
namespace {
void require(bool ok, const char* reason) { if (!ok) throw Error(reason); }
void check(const std::atomic_bool* c) {
  if (c && c->load(std::memory_order_relaxed)) throw Cancelled();
}
bool bounded(const std::string& s) { return !s.empty() && s.size() <= 256; }
bool validSha(const std::string& s) {
  if (s.size() != 64) return false;
  for (unsigned char c : s) if (!std::isxdigit(c)) return false;
  return true;
}
void checkExpected(const Provenance& p, std::string_view test) {
  require(test == "TEST1" || test == "TEST2", "Invalid COM Test case");
  require(validSha(p.sourceSha256) && bounded(p.revision) &&
          bounded(p.mappingIdentity) && bounded(p.settingsIdentity) &&
          p.workspaceGeneration > 0 && p.mappingGeneration > 0,
          "Missing voltage-ratio source or mapping identity");
  const std::string token = "|case=" + std::string(test) + "|";
  const auto pos = p.settingsIdentity.find(token);
  require(pos != std::string::npos &&
          p.settingsIdentity.find(token, pos + token.size()) == std::string::npos &&
          p.settingsIdentity.find(test == "TEST1" ? "|case=TEST2|" :
                                  "|case=TEST1|") == std::string::npos,
          "Mixed, duplicated or missing Test case identity");
}
void checkEvidence(const Result<ComVoltageEvidence>& r,
                   const Provenance& p, std::string_view test) {
  require(r.usable(), "Missing or unusable upstream amplitude");
  const auto& q = r.provenance;
  require(q.sourceSha256 == p.sourceSha256 && q.revision == p.revision &&
          q.mappingIdentity == p.mappingIdentity &&
          q.settingsIdentity == p.settingsIdentity &&
          q.workspaceGeneration == p.workspaceGeneration &&
          q.mappingGeneration == p.mappingGeneration &&
          bounded(q.algorithm) && bounded(q.version),
          "Upstream amplitude provenance mismatch");
  const auto& e = *r.value;
  require(e.testCase == test && bounded(e.referencePlane) &&
          bounded(e.calibrationIdentity),
          "Missing test, receiver plane or voltage calibration");
  require(std::isfinite(e.amplitudeV) && e.amplitudeV > 0.,
          "Signal/noise voltage must be finite and positive");
}
double safeDb(double a, double b) {
  // Prefer direct log of a representable ratio near unity.
  // Subtract logs if division underflows/overflows.
  const double q = a / b;
  const double db = std::isfinite(q) && q > 0. ?
      20. * std::log10(q) :
      (20. / std::log(10.)) * (std::log(a) - std::log(b));
  require(std::isfinite(db), "Invalid logarithmic voltage ratio");
  return db;
}
void sameSource(const Provenance& a, const Provenance& b) {
  require(a.sourceSha256 == b.sourceSha256 && a.revision == b.revision &&
          a.mappingIdentity == b.mappingIdentity &&
          a.workspaceGeneration == b.workspaceGeneration &&
          a.mappingGeneration == b.mappingGeneration,
          "Test1/Test2 source identity mismatch");
}
}
Result<ComVoltageRatioData> calculateComDiagnosticVoltageRatio(
    const ComVoltageRatioInput& input, std::string_view test,
    const Provenance& expected, const std::atomic_bool* cancelled) {
  check(cancelled);
  checkExpected(expected, test);
  checkEvidence(input.signal, expected, test);
  checkEvidence(input.noiseAtTargetDer, expected, test);
  const auto& a = *input.signal.value;
  const auto& b = *input.noiseAtTargetDer.value;
  require(a.referencePlane == b.referencePlane &&
          a.calibrationIdentity == b.calibrationIdentity,
          "Different voltage receiver planes or calibrations");
  const double db = safeDb(a.amplitudeV, b.amplitudeV);
  check(cancelled);
  Result<ComVoltageRatioData> out;
  out.status = Status::Warning;
  out.value = ComVoltageRatioData{a.amplitudeV, b.amplitudeV, db,
      std::string(test), a.referencePlane, a.calibrationIdentity};
  out.provenance = expected;
  out.provenance.algorithm = "com-voltage-ratio-diagnostic";
  out.provenance.version = "012_19-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Mathematical ratio only. Annex 93A signal/noise DER calibration, "
      "statistics and optimization are unverified; no IEEE COM/PHY verdict."});
  return out;
}
Result<ComVoltageRatioCasesData> calculateComDiagnosticVoltageRatioCases(
    const ComVoltageRatioInput& test1, const ComVoltageRatioInput& test2,
    const Provenance& p1, const Provenance& p2,
    const std::atomic_bool* cancelled) {
  check(cancelled);
  checkExpected(p1, "TEST1");
  checkExpected(p2, "TEST2");
  sameSource(p1, p2);
  const auto a = calculateComDiagnosticVoltageRatio(test1, "TEST1", p1, cancelled);
  const auto b = calculateComDiagnosticVoltageRatio(test2, "TEST2", p2, cancelled);
  check(cancelled);
  Result<ComVoltageRatioCasesData> out;
  out.status = Status::Warning;
  out.value = ComVoltageRatioCasesData{
      *a.value, *b.value, std::min(a.value->ratioDb, b.value->ratioDb)};
  out.provenance = p1;
  out.provenance.settingsIdentity = "EXPLORATORY_TEST1_TEST2_PAIR";
  out.provenance.algorithm = "com-voltage-ratio-diagnostic";
  out.provenance.version = "012_19-1";
  out.diagnostics.push_back({Severity::Warning, "NOT_IEEE_COM",
      "Minimum of two mathematical ratios; NOT IEEE PHY PASS/FAIL."});
  return out;
}
} // namespace si::channel
