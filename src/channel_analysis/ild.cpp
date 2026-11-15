#include "si/channel_analysis/ild.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace si::channel {
namespace {
constexpr size_t maximumPoints = 1u << 20;
void require(bool ok, const char *message) { if (!ok) throw Error(message); }
void check(const std::atomic_bool *flag) {
  if (flag && flag->load(std::memory_order_relaxed)) throw Cancelled();
}
size_t terms(IldFitModel m) {
  if (m == IldFitModel::Linear) return 2;
  if (m == IldFitModel::RootLinear) return 3;
  return 4;
}
std::array<double,4> basis(double normalized, IldFitModel model) {
  const double root = std::sqrt(normalized);
  if (model == IldFitModel::Linear) return {1, normalized, 0, 0};
  if (model == IldFitModel::RootLinear) return {1, root, normalized, 0};
  return {1, root, normalized, normalized * normalized};
}
}
const char *ildModelName(IldFitModel m) {
  switch (m) {
    case IldFitModel::Linear: return "a0 + a1*(f/F)";
    case IldFitModel::RootLinear: return "a0 + a1*sqrt(f/F) + a2*(f/F)";
    case IldFitModel::RootLinearQuadratic:
      return "a0 + a1*sqrt(f/F) + a2*(f/F) + a3*(f/F)^2";
  }
  throw Error("Unsupported ILD fitting model");
}
Result<IldData> calculateIld(const ComplexTrace &trace, const IldSettings &settings,
                             Provenance provenance, const std::atomic_bool *cancelled) {
  check(cancelled);
  validateTrace(trace);
  require(std::isfinite(settings.startHz) && std::isfinite(settings.stopHz) &&
          settings.startHz >= 0 && settings.stopHz > settings.startHz,
          "Invalid ILD frequency range");
  // Do not extrapolate; the selected range is intersected with measured bins.
  const size_t width = terms(settings.model);
  const char *modelLabel = ildModelName(settings.model);
  const auto first = std::lower_bound(trace.frequencyHz.begin(), trace.frequencyHz.end(),
                                     settings.startHz);
  const auto last = std::upper_bound(first, trace.frequencyHz.end(), settings.stopHz);
  const size_t n = size_t(last - first);
  require(n > width && n <= maximumPoints, "Insufficient or excessive ILD fit samples");
  const size_t offset = size_t(first - trace.frequencyHz.begin());
  const double scale = *(last - 1);
  require(scale > 0 && std::isfinite(scale), "Invalid ILD frequency scale");
  IldData value;
  value.parameter = trace.parameter;
  value.modelName = modelLabel;
  value.frequencyScaleHz = scale;
  value.rangeStartHz = *first;
  value.rangeStopHz = *(last - 1);
  value.frequencyHz.reserve(n);
  value.insertionLossDb.reserve(n);
  value.fittedLossDb.reserve(n);
  value.deviationDb.reserve(n);
  // Construct a bounded, normalized matrix and solve with twice-reorthogonalized
  // modified Gram-Schmidt QR. Never invert the ill-conditioned normal equations.
  std::array<std::vector<double>,4> columns;
  for (size_t j=0;j<width;++j) columns[j].reserve(n);
  for (size_t i=0;i<n;++i) {
    if (!(i % 1024)) check(cancelled);
    const double f=trace.frequencyHz[offset+i];
    const double mag=std::abs(trace.transfer[offset+i]);
    require(std::isfinite(mag) && mag>0, "Zero or nonfinite ILD transfer magnitude");
    const double loss=-20.0*std::log10(mag);
    require(std::isfinite(loss), "Nonfinite insertion loss");
    value.frequencyHz.push_back(f);
    value.insertionLossDb.push_back(loss);
    auto b=basis(f/scale, settings.model);
    for(size_t j=0;j<width;++j) columns[j].push_back(b[j]);
  }
  std::array<std::array<double,4>,4> r{};
  const double threshold=1e-10;
  const double initialNorm=std::sqrt(double(n));
  for(size_t j=0;j<width;++j) {
    check(cancelled);
    for(size_t k=0;k<j;++k) {
      for(int pass=0;pass<2;++pass) {
        double dot=0;
        for(size_t i=0;i<n;++i) dot+=columns[k][i]*columns[j][i];
        r[k][j]+=dot;
        for(size_t i=0;i<n;++i) columns[j][i]-=dot*columns[k][i];
      }
    }
    double norm2=0;
    for(double item:columns[j]) norm2+=item*item;
    r[j][j]=std::sqrt(norm2);
    require(std::isfinite(r[j][j]) && r[j][j]>threshold*initialNorm,
            "Rank deficient or ill-conditioned ILD fitting matrix");
    for(double &item:columns[j]) item/=r[j][j];
  }
  std::array<double,4> c{};
  for(size_t j=0;j<width;++j) {
    check(cancelled);
    for(size_t i=0;i<n;++i) c[j]+=columns[j][i]*value.insertionLossDb[i];
  }
  for(int j=int(width)-1;j>=0;--j) {
    double rhs=c[size_t(j)];
    for(size_t k=size_t(j)+1;k<width;++k) rhs-=r[size_t(j)][k]*value.coefficients[k];
    value.coefficients[size_t(j)]=rhs/r[size_t(j)][size_t(j)];
    require(std::isfinite(value.coefficients[size_t(j)]), "Nonfinite ILD fit coefficient");
  }
  value.minimumDb=std::numeric_limits<double>::infinity();
  value.maximumDb=-value.minimumDb;
  double sumSquares=0;
  for(size_t i=0;i<n;++i) {
    if (!(i % 1024)) check(cancelled);
    const auto b=basis(value.frequencyHz[i]/scale, settings.model);
    double fitted=0;
    for(size_t j=0;j<width;++j) fitted+=b[j]*value.coefficients[j];
    const double residual=value.insertionLossDb[i]-fitted;
    require(std::isfinite(residual), "Invalid ILD residual");
    value.fittedLossDb.push_back(fitted);
    value.deviationDb.push_back(residual);
    if(residual<value.minimumDb) value.minimumDb=residual;
    if(residual>value.maximumDb) value.maximumDb=residual;
    if(std::abs(residual)>value.peakAbsoluteDb) {
      value.peakAbsoluteDb=std::abs(residual);
      value.peakFrequencyHz=value.frequencyHz[i];
    }
    sumSquares+=residual*residual;
  }
  value.rmsDb=std::sqrt(sumSquares/double(n));
  require(std::isfinite(value.rmsDb), "Invalid ILD RMS");
  Result<IldData> result;
  result.provenance=std::move(provenance);
  result.status=Status::Valid;
  result.value=std::move(value);
  if(trace.frequencyHz.front() < settings.startHz ||
     trace.frequencyHz.back() > settings.stopHz ||
     trace.frequencyHz.front() > settings.startHz ||
     trace.frequencyHz.back() < settings.stopHz) {
    result.diagnostics.push_back({Severity::Info,"FIT_INTERSECTION",
      "Only measured frequency bins within the selected range are used; no extrapolation"});
  }
  return result;
}
} // namespace si::channel
