void MainWindow::run(std::vector<si::Metric> metrics) {
  if (busy)
    return;
  try {
    syncSettings();
    if (metrics.empty())
      throw si::Error("Select at least one analysis type.");
    auto selected = selectedChannels();
    if (selected.empty())
      throw si::Error("Select channels to analyze.");
    for (auto &r : revisions) {
      if (!r.confirmed)
        throw si::Error("Mapping confirmation required: " + ss(r.name));
      si::validateMapping(r.channels, r.cache->meta());
    }
    auto jobs = std::make_shared<std::vector<si::Job>>();
  auto appendDirections = [&](si::Job base) {
    for (bool reverse : {false, true}) {
      if (!si::directionAvailable(base, reverse)) continue;
      auto job = base;
      job.reverse = reverse;
      jobs->push_back(std::move(job));
    }
  };
  for (auto &rev : revisions) {
    auto matches = si::matchChannels(selected, rev.channels);
    for (size_t i = 0; i < selected.size(); ++i) {
      if (matches[i] < 0) continue;
      const si::Channel victim = rev.channels[size_t(matches[i])];
      for (auto metric : metrics) {
        if (metric == si::Metric::NEXT || metric == si::Metric::FEXT) {
          for (const auto &a : rev.channels) {
            if (a.id == victim.id || a.group != victim.group) continue;
            if (aggressorBox->currentIndex() != 0 &&
                qs(a.name) != aggressorBox->currentText()) continue;
            appendDirections(si::Job{rev.cache, victim, a, metric, ss(rev.name)});
          }
        } else {
          appendDirections(si::Job{rev.cache, victim, std::nullopt, metric,
                                   ss(rev.name)});
        }
      }
    }
  }
  if (jobs->empty())
    throw si::Error("No selected channels have valid mapped endpoints for the requested analysis.");
    *jobs=si::expandTdrJobs(*jobs,settings);
    auto set = settings;
    auto output = std::make_shared<std::vector<si::Result>>();
    displayed = metrics.front();
    task(
        [this, jobs, set, output](si::Control *c) {
          *output =
              si::runJobs(*jobs, set, c,
                          [this](size_t n, const std::function<void()> &work) { analysisPool.run(n, work); });
          for (size_t i = 0; i < jobs->size() && i < output->size(); ++i) {
          const auto &job = (*jobs)[i];
          if (!job.reverse || *job.reverse ||
              (job.metric != si::Metric::RL && job.metric != si::Metric::IL))
            continue;
          auto same = [&](const si::Job &other) {
            return other.reverse && *other.reverse && other.cache == job.cache &&
                   other.metric == job.metric && other.termination == job.termination &&
                   other.revision == job.revision && other.victim.id == job.victim.id &&
                   (other.aggressor ? other.aggressor->id : "") ==
                       (job.aggressor ? job.aggressor->id : "");
          };
          auto it = std::find_if(jobs->begin(), jobs->end(), same);
          if (it == jobs->end()) continue;
          const size_t j = size_t(it - jobs->begin());
          if (j >= output->size() || (*output)[i].plotX.empty() ||
              (*output)[j].plotX.empty())
            continue;
          try {
            const auto delta = si::directionDelta(*job.cache, job.victim,
                                                  job.metric, set, c);
            (*output)[i].directionDelta = (*output)[j].directionDelta = delta.maximumDb;
            (*output)[i].directionDeltaX = (*output)[j].directionDeltaX = delta.frequency;
          } catch (const si::Error &) {
            // Directional delta is diagnostic. Never turn an otherwise valid
            // one-direction analysis into a failed run when the pair is unavailable.
          }
        }
        },
        [this, jobs, set, output] {
          lastJobs = std::move(*jobs);
          results = std::move(*output);
          lastSettings = set;
          dirty = true;
          refreshResults();
          showMetric(displayed);
          notice->setText("Analysis complete. Frequency/time and margin values use full-resolution data. TDR starts from the lowest source frequency; check the preview quality notes.");
        });
  } catch (const std::exception &e) {
    error(qs(e.what()));
  }
}
