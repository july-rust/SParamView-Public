size_t workerLimit(int performance, size_t jobs, unsigned hardwareThreads) {
  if (performance < 0 || performance > 2)
    throw Error("Performance setting must be 0, 1, or 2");
  if (!jobs)
    return 0;
  const size_t hardware = std::max(1u, hardwareThreads);
  const size_t requested = performance == 0 ? 1 : performance == 1 ? 2 : 8;
  return std::min({requested, hardware, jobs});
}
void rankResults(std::vector<Result> &r, const std::string &by) {
  std::stable_sort(r.begin(), r.end(), [&](const auto &a, const auto &b) {
    if (a.metric != b.metric)
      return a.metric < b.metric;
    if (by == "Channel")
      return a.channel < b.channel;
    if (by == "Result") {
      auto priority = [](auto s) { return s == "NG" ? 0 : s == "N/A" ? 1 : 2; };
      if (a.status != b.status)
        return priority(a.status) < priority(b.status);
    }
    if (by == "Margin") {
      const bool aHasMargin = !std::isnan(a.margin);
      const bool bHasMargin = !std::isnan(b.margin);
      // Keep one comparison policy for every pair. Mixing margin ordering for
      // finite/finite pairs with severity ordering for finite/missing pairs can
      // violate strict weak ordering (A < B < C < A), which makes std::sort
      // behavior undefined. Defined margins sort first; missing margins form a
      // separate equivalence class and fall back to severity only within it.
      if (aHasMargin != bHasMargin)
        return aHasMargin;
      if (aHasMargin && a.margin != b.margin)
        return a.margin < b.margin;
    }
    auto va = severity(a.metric, a.worst, a.targetOhm),
         vb = severity(b.metric, b.worst, b.targetOhm);
    if (std::isnan(va))
      return false;
    if (std::isnan(vb))
      return true;
    return va > vb;
  });
}
