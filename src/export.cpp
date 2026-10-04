void exportRawCsv(const fs::path &p, const Trace &t, Metric m,
                  const Settings &settings, Control *c) {
  auto tmpPath = p;
  tmpPath += ".tmp";
  Temporary tmp{tmpPath};
  std::ofstream out(tmp.path, std::ios::binary);
  if (!out)
    throw Error("Cannot create raw CSV");
  out << std::setprecision(17) << "\xef\xbb\xbf";
  if (m == Metric::TDR) {
    auto td = transformTdr(crop(t, t.x.front(), settings.stopHz), settings, c);
    if (td.time.empty())
      throw Error(td.note);
    out << "Time_s,Impedance_ohm,Reflection_rho,Termination\r\n";
    for (size_t i = 0; i < td.time.size(); ++i) {
      check(c);
      out << td.time[i] << ',' << td.impedance[i] << ',' << td.reflection[i] << ',' << name(t.termination,t.parameter.starts_with("Sdd")) << "\r\n";
    }
  } else {
    auto a = crop(t, settings.startHz, settings.stopHz);
    out << "Frequency_Hz,Real,Imaginary,LogMagnitude_dB,Phase_deg\r\n";
    for (size_t i = 0; i < a.x.size(); ++i) {
      if (i % 8192 == 0)
        check(c);
      out << a.x[i] << ',' << a.s[i].real() << ',' << a.s[i].imag() << ','
          << logMagnitude(a.s[i]) << ',' << std::arg(a.s[i]) * 180 / pi
          << "\r\n";
    }
  }
  out.close();
  if (!out)
    throw Error("CSV write failed");
  check(c);
  atomicReplace(tmp.path, p);
}
