#include "app.hpp"
#include <QtEndian>
#include <map>
#include <set>
namespace {
class Zip {
  QSaveFile file;
  struct Entry {
    QByteArray name;
    quint32 crc, size, offset;
  };
  std::vector<Entry> entries;
  void word(quint16 n) {
    char b[2];
    qToLittleEndian(n, b);
    if (file.write(b, 2) != 2)
      throw si::Error("ZIP write failed");
  }
  void dword(quint32 n) {
    char b[4];
    qToLittleEndian(n, b);
    if (file.write(b, 4) != 4)
      throw si::Error("ZIP write failed");
  }

public:
  explicit Zip(const QString &path) : file(path) {
    if (!file.open(QIODevice::WriteOnly))
      throw si::Error(ss(file.errorString()));
  }
  void add(const QString &name, QIODevice &input, si::Control *control) {
    if (file.pos() > 0xffffffffLL || input.size() > 0xffffffffLL)
      throw si::Error("Report exceeds ZIP32; export raw data to CSV");
    auto n = name.toUtf8();
    Entry e{n, 0, 0, quint32(file.pos())};
    dword(0x04034b50);
    word(20);
    word(0x808);
    word(0);
    word(0);
    word(0x21);
    dword(0);
    dword(0);
    dword(0);
    word(quint16(n.size()));
    word(0);
    file.write(n);
    quint32 crc = 0xffffffff;
    input.seek(0);
    while (!input.atEnd()) {
      if (control)
        control->check();
      auto data = input.read(65536);
      if (data.isEmpty() && !input.atEnd())
        throw si::Error("Report read failed");
      for (unsigned char c : data) {
        crc ^= c;
        for (int k = 0; k < 8; ++k)
          crc = (crc >> 1) ^ (0xedb88320u & quint32(-int(crc & 1)));
      }
      if (file.write(data) != data.size())
        throw si::Error("Report write failed");
      e.size += quint32(data.size());
    }
    e.crc = crc ^ 0xffffffff;
    dword(0x08074b50);
    dword(e.crc);
    dword(e.size);
    dword(e.size);
    entries.push_back(e);
  }
  void add(const QString &name, const QByteArray &data,
           si::Control *c = nullptr) {
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    add(name, buffer, c);
  }
  void finish() {
    if (entries.size() > 65535 || file.pos() > 0xffffffffLL)
      throw si::Error("Report exceeds ZIP32 capacity");
    auto start = quint32(file.pos());
    for (auto &e : entries) {
      dword(0x02014b50);
      word(20);
      word(20);
      word(0x808);
      word(0);
      word(0);
      word(0x21);
      dword(e.crc);
      dword(e.size);
      dword(e.size);
      word(quint16(e.name.size()));
      word(0);
      word(0);
      word(0);
      word(0);
      dword(0);
      dword(e.offset);
      file.write(e.name);
    }
    auto bytes = quint32(file.pos()) - start;
    dword(0x06054b50);
    word(0);
    word(0);
    word(quint16(entries.size()));
    word(quint16(entries.size()));
    dword(bytes);
    dword(start);
    word(0);
    if (!file.commit())
      throw si::Error(ss(file.errorString()));
  }
};
const QString ns = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
QString clean(QString s) {
  for (qsizetype i = s.size(); i-- > 0;)
    if (s[i].unicode() < 32 && s[i] != '\t' && s[i] != '\n' && s[i] != '\r')
      s.remove(i, 1);
  return s.left(32767);
}
QString column(int i) {
  QString s;
  do {
    s.prepend(QChar('A' + i % 26));
    i = i / 26 - 1;
  } while (i >= 0);
  return s;
}
class Sheet {
public:
  QTemporaryFile file;
  QXmlStreamWriter xml;
  int row = 0;
  Sheet() : xml(&file) {
    if (!file.open())
      throw si::Error("Cannot create report staging file");
    xml.writeStartDocument();
    xml.writeStartElement("worksheet");
    xml.writeDefaultNamespace(ns);
    xml.writeNamespace(
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
        "r");
    xml.writeStartElement("sheetViews");
    xml.writeStartElement("sheetView");
    xml.writeAttribute("workbookViewId", "0");
    xml.writeEmptyElement("pane");
    xml.writeAttribute("ySplit", "1");
    xml.writeAttribute("topLeftCell", "A2");
    xml.writeAttribute("activePane", "bottomLeft");
    xml.writeAttribute("state", "frozen");
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeStartElement("cols");
    xml.writeEmptyElement("col");
    xml.writeAttribute("min", "1");
    xml.writeAttribute("max", "30");
    xml.writeAttribute("width", "22");
    xml.writeAttribute("customWidth", "1");
    xml.writeEndElement();
    xml.writeStartElement("sheetData");
  }
  void append(const QVariantList &values, bool header = false) {
    if (row >= 1048576)
      throw si::Error("Excel row limit reached; use raw CSV export");
    ++row;
    xml.writeStartElement("row");
    xml.writeAttribute("r", QString::number(row));
    for (qsizetype i = 0; i < values.size(); ++i) {
      auto v = values[i];
      xml.writeStartElement("c");
      xml.writeAttribute("r", column(int(i)) + QString::number(row));
      int style = header                 ? 1
                  : v.toString() == "NG" ? 3
                  : v.toString() == "OK" ? 2
                                         : 0;
      xml.writeAttribute("s", QString::number(style));
      if (v.metaType().id() == QMetaType::Double && std::isfinite(v.toDouble()))
        xml.writeTextElement("v", QString::number(v.toDouble(), 'g', 17));
      else {
        xml.writeAttribute("t", "inlineStr");
        xml.writeStartElement("is");
        QString s = v.metaType().id() == QMetaType::Double ? num(v.toDouble())
                                                           : v.toString();
        xml.writeTextElement("t", clean(s));
        xml.writeEndElement();
      }
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  void end(bool drawings = false) {
    xml.writeEndElement();
    if (drawings) {
      xml.writeEmptyElement("drawing");
      xml.writeAttribute("r:id", "rId1");
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    file.flush();
    if (xml.hasError())
      throw si::Error("Report XML write failed");
  }
};
QByteArray xmlBytes(const std::function<void(QXmlStreamWriter &)> &f) {
  QByteArray bytes;
  QXmlStreamWriter w(&bytes);
  w.writeStartDocument();
  f(w);
  w.writeEndDocument();
  return bytes;
}
} // namespace
void writeReport(const QString &path, const std::vector<si::Result> &results,
                 const std::vector<Revision> &revisions,
                 const si::Settings &settings, const QString &project,
                 const ReportOptions &options, const std::vector<si::Job> &jobs,
                 const PlotSnapshot &current, si::Control *control) {
  if (!path.endsWith(".xlsx", Qt::CaseInsensitive))
    throw si::Error("Report filename must end in .xlsx");
  for (auto &r : revisions)
    if (QFileInfo(path).absoluteFilePath() ==
        QFileInfo(r.source).absoluteFilePath())
      throw si::Error("Original Touchstone is read-only");
  Zip zip(path);
  QStringList sheetNames;
  int graphSheet = 0, imageCount = 0;
  auto addSheet = [&](const QString &name, Sheet &sheet, bool drawing = false) {
    sheet.end(drawing);
    sheetNames << name;
    zip.add("xl/worksheets/sheet" + QString::number(sheetNames.size()) + ".xml",
            sheet.file, control);
  };
  if (options.summary) {
    Sheet sheet;
    sheet.append(
        {"Channel", "Revision", "RL", "IL", "NEXT", "FEXT", "TDR", "Overall"},
        true);
    std::map<std::pair<std::string, std::string>,
             std::map<si::Metric, std::vector<std::string>>>
        groups;
    for (auto &r : results)
      groups[{r.channel, r.revision}][r.metric].push_back(r.status);
    for (auto &[key, metrics] : groups) {
      QVariantList row{qs(key.first), qs(key.second)};
      std::vector<std::string> all;
      for (auto m : {si::Metric::RL, si::Metric::IL, si::Metric::NEXT,
                     si::Metric::FEXT, si::Metric::TDR}) {
        auto status = metrics.count(m) ? si::overall(metrics[m]) : "N/A";
        row << qs(status);
        if (metrics.count(m))
          all.push_back(status);
      }
      row << qs(si::overall(all));
      sheet.append(row);
    }
    addSheet("Summary", sheet);
  }
  for (auto metric : {si::Metric::RL, si::Metric::IL, si::Metric::NEXT,
                      si::Metric::FEXT, si::Metric::TDR})
    if (options.details) {
      std::vector<const si::Result *> rows;
      for (auto &r : results)
        if (r.metric == metric)
          rows.push_back(&r);
      if (rows.empty())
        continue;
      Sheet sheet;
      sheet.append({"Channel", "Revision", "Metric", "Aggressor", "Worst value",
                    "Worst Frequency Hz / Time s", "Limit at worst margin",
                    "Worst margin", "Margin Frequency Hz / Time s", "Result",
                    "Direction", "Minimum", "Maximum", "Target ohm",
                    "Reference ohm", "TDR quality", "Note", "Min error %",
                    "Max error %", "Max directional delta dB",
                    "Directional delta frequency Hz", "Termination", "Value unit"},
                   true);
      for (auto r : rows) {
        if (control)
          control->check();
        sheet.append({qs(r->channel), qs(r->revision),
                      metric == si::Metric::TDR
                          ? (r->reflection ? "Reflection rho (from " : "Impedance (from ") + qs(r->parameter) + (r->reflection ? ") [1]" : ") [ohm]")
                          : qs(r->parameter) + " [dB]",
                      qs(r->aggressor), r->worst, r->worstX, r->limitAtMargin,
                      r->margin, r->marginX, qs(r->status), qs(r->direction),
                      r->minimum, r->maximum, r->targetOhm, r->referenceOhm,
                      qs(r->quality), qs(r->note), r->minErrorPercent,
                      r->maxErrorPercent, r->directionDelta, r->directionDeltaX,
                      metric==si::Metric::TDR ? qs(si::name(r->termination,r->parameter.starts_with("Sdd"))) : "",
                      metric==si::Metric::TDR ? (r->reflection?"rho":"ohm") : "dB"});
      }
      QString title = metric == si::Metric::RL   ? "Return_Loss"
                      : metric == si::Metric::IL ? "Insertion_Loss"
                                                 : qs(si::name(metric));
      addSheet(title, sheet);
    }
  {
    Sheet config;
    config.append({"Item", "Value"}, true);
    config.append({"Project", project});
    config.append({"Program version", si::version});
    config.append(
        {"Analysis date", QDateTime::currentDateTime().toString(Qt::ISODate)});
    config.append({"Settings JSON",
                   QString::fromUtf8(QJsonDocument(settingsJson(settings))
                                         .toJson(QJsonDocument::Compact))});
    config.append(
        {"TDR algorithm",
         "Low-pass step; Kaiser beta 6 default; complex linear resampling; "
         "automatic DC extrapolation; source Z0 retained. If the FFT grid loses "
         "response: direct nonuniform low-pass step with convergence and "
         "source interpolation sensitivity checks (LIMITED)."});
    config.append({"TDR validation",
                   "Preview: synthetic/reference-library checks; "
                   "PowerSI / VNA acceptance pending"});
    config.append({"dB convention", "20*log10(abs(S)); RL/NEXT/FEXT pass value "
                                    "<= limit; IL pass value >= limit"});
    config.append(
        {"Crosstalk direction",
         "Forward: NEXT Aggressor Near -> Victim Near; FEXT Aggressor Near -> "
         "Victim Far. Reverse starts at Aggressor Far."});
    config.append({"TDR quality meaning",
                   "GOOD/LIMITED/UNSUITABLE describe transform suitability, "
                   "not device compliance or guaranteed accuracy."});
    config.append({"Delta convention",
                   "Revision delta: candidate dB - baseline dB. Direction delta: "
                   "absolute N->F versus F->N difference for RL/IL; diagnostic only."});
    config.append(
        {"Raw trace data",
         options.raw
             ? "Included in Raw_* sheets"
             : "Excluded (default); selected traces can be exported as CSV"});
    for (auto &r : revisions) {
      auto &m = r.cache->meta();
      config.append({"Revision", r.name});
      config.append({"Source", r.source});
      config.append({"Source modified",
                     QFileInfo(r.source).lastModified().toString(Qt::ISODate)});
      config.append({"Source size bytes", QString::number(m.sourceSize)});
      config.append({"Source SHA-256", qs(m.sha256)});
      config.append({"Touchstone", qs(m.version) + " / " + qs(m.format) +
                                       " / " + qs(m.matrix)});
      for (auto &c : r.channels)
        config.append({"Channel mapping",
                       QString::fromUtf8(QJsonDocument(channelJson(c))
                                             .toJson(QJsonDocument::Compact))});
    }
    for (auto &r : results)
      if (r.metric == si::Metric::TDR)
        config.append({"TDR " + qs(r.channel) + " / " + qs(r.revision),
                       qs(r.quality) + "; df=" + num(r.transformDf) +
                           " Hz; fmax=" + num(r.transformFmax) + " Hz; " +
                           qs(r.note)});
    addSheet("Configuration", config);
  }
  if (options.raw) {
    int part = 1;
    auto sheet = std::make_unique<Sheet>();
    auto startRaw = [&]() {
      sheet->append({"Channel", "Revision", "Analysis", "Direction", "Aggressor",
                     "Frequency Hz / Time s", "Real", "Imaginary", "Value", "Unit", "Termination"},
                    true);
    };
    startRaw();
    for (auto &j : jobs) {
      if (control)
        control->check();
      try {
        auto jobSettings = settings; jobSettings.reverse = j.reverse.value_or(settings.reverse);
        auto t = si::loadTrace(*j.cache, j.victim, j.metric, jobSettings,
                               j.aggressor ? &*j.aggressor : nullptr, control, j.termination);
        std::vector<double> x, y;
        std::vector<si::Complex> z;
        if (j.metric == si::Metric::TDR) {
          auto td = si::transformTdr(si::crop(t, t.x.front(), jobSettings.stopHz),
                                     jobSettings, control);
          x = std::move(td.time);
          y = jobSettings.tdrReflection ? std::move(td.reflection) : std::move(td.impedance);
        } else {
          t = si::crop(t, jobSettings.startHz, jobSettings.stopHz);
          x = std::move(t.x);
          z = std::move(t.s);
          for (auto s : z)
            y.push_back(si::logMagnitude(s));
        }
        for (size_t i = 0; i < x.size(); ++i) {
          if (i % 8192 == 0 && control)
            control->check();
          if (sheet->row >= 1000000) {
            addSheet("Raw_" + QString::number(part++), *sheet);
            sheet = std::make_unique<Sheet>();
            startRaw();
          }
          sheet->append({qs(j.victim.name), qs(j.revision),
                         qs(si::name(j.metric)), qs(si::metricDirection(j.metric, jobSettings.reverse)),
                         j.aggressor ? qs(j.aggressor->name) : "", x[i],
                         z.empty() ? QVariant() : QVariant(z[i].real()),
                         z.empty() ? QVariant() : QVariant(z[i].imag()), y[i],
                         j.metric==si::Metric::TDR ? (jobSettings.tdrReflection?"rho":"ohm") : "dB",
                         j.metric==si::Metric::TDR ? qs(si::name(j.termination,j.victim.differential())) : ""});
        }
      } catch (const si::Cancelled &) {
        throw;
      } catch (const std::exception &e) {
        sheet->append({qs(j.victim.name), qs(j.revision), "N/A", qs(e.what())});
      }
    }
    addSheet("Raw_" + QString::number(part), *sheet);
  }
  if (options.graphs) {
    Sheet graph;
    graph.append(
        {"Graphs",
         "Current view followed by result graphs (up to 8 traces per image)"},
        true);
    auto emitGraph = [&](const PlotSnapshot &snapshot) {
      if (control)
        control->check();
      auto image = graphImage(snapshot);
      QByteArray png;
      QBuffer buffer(&png);
      buffer.open(QIODevice::WriteOnly);
      if (!image.save(&buffer, "PNG"))
        throw si::Error("Graph encoding failed");
      zip.add("xl/media/image" + QString::number(++imageCount) + ".png", png,
              control);
      for (int i = 0; i < 28; ++i)
        graph.append({});
    };
    if (!current.curves.empty())
      emitGraph(current);
    for (auto metric : {si::Metric::RL, si::Metric::IL, si::Metric::NEXT,
                        si::Metric::FEXT, si::Metric::TDR}) {
      PlotSnapshot snapshot;
      snapshot.project = project;
      snapshot.settings = settings;
      snapshot.metric = metric; snapshot.bothDirections = true;
      for (const auto &result : results) {
        if (result.metric != metric || std::isnan(result.worst))
          continue;
        if (control)
          control->check();
        si::Result curve = result;
        if (curve.plotX.empty()) {
          auto it =
              std::find_if(jobs.begin(), jobs.end(), [&](const auto &job) {
                return job.metric == curve.metric && job.termination == curve.termination &&
                       job.revision == curve.revision &&
                       job.victim.id == curve.channelId &&
                       (job.aggressor ? job.aggressor->id : "") ==
                           curve.aggressorId && job.reverse.value_or(settings.reverse) == curve.reverse;
              });
          if (it == jobs.end())
            throw si::Error("Report source job is missing");
          auto jobSettings = settings; jobSettings.reverse = it->reverse.value_or(settings.reverse);
          auto trace = si::loadTrace(*it->cache, it->victim, it->metric, jobSettings,
                            it->aggressor ? &*it->aggressor : nullptr, control, it->termination);
          auto display = si::analyze(trace, metric, jobSettings, control);
          curve.plotX = std::move(display.plotX);
          curve.plotY = std::move(display.plotY);
        }
        snapshot.curves.push_back(std::move(curve));
        if (snapshot.curves.size() == 8) {
          emitGraph(snapshot);
          snapshot.curves.clear();
        }
      }
      if (!snapshot.curves.empty())
        emitGraph(snapshot);
    }
    graphSheet = sheetNames.size() + 1;
    addSheet("Graphs", graph, imageCount > 0);
  }
  if (imageCount) {
    zip.add("xl/drawings/drawing1.xml", xmlBytes([&](QXmlStreamWriter &w) {
              w.writeStartElement("xdr:wsDr");
              w.writeNamespace("http://schemas.openxmlformats.org/drawingml/"
                               "2006/spreadsheetDrawing",
                               "xdr");
              w.writeNamespace(
                  "http://schemas.openxmlformats.org/drawingml/2006/main", "a");
              w.writeNamespace("http://schemas.openxmlformats.org/"
                               "officeDocument/2006/relationships",
                               "r");
              for (int i = 0; i < imageCount; ++i) {
                w.writeStartElement("xdr:oneCellAnchor");
                w.writeStartElement("xdr:from");
                w.writeTextElement("xdr:col", "0");
                w.writeTextElement("xdr:colOff", "0");
                w.writeTextElement("xdr:row", QString::number(1 + i * 28));
                w.writeTextElement("xdr:rowOff", "0");
                w.writeEndElement();
                w.writeEmptyElement("xdr:ext");
                w.writeAttribute("cx", "8001000");
                w.writeAttribute("cy", "4667250");
                w.writeStartElement("xdr:pic");
                w.writeStartElement("xdr:nvPicPr");
                w.writeEmptyElement("xdr:cNvPr");
                w.writeAttribute("id", QString::number(i + 1));
                w.writeAttribute("name", "SI Graph " + QString::number(i + 1));
                w.writeEmptyElement("xdr:cNvPicPr");
                w.writeEndElement();
                w.writeStartElement("xdr:blipFill");
                w.writeEmptyElement("a:blip");
                w.writeAttribute("r:embed", "rId" + QString::number(i + 1));
                w.writeStartElement("a:stretch");
                w.writeEmptyElement("a:fillRect");
                w.writeEndElement();
                w.writeEndElement();
                w.writeStartElement("xdr:spPr");
                w.writeStartElement("a:prstGeom");
                w.writeAttribute("prst", "rect");
                w.writeEmptyElement("a:avLst");
                w.writeEndElement();
                w.writeEndElement();
                w.writeEndElement();
                w.writeEmptyElement("xdr:clientData");
                w.writeEndElement();
              }
              w.writeEndElement();
            }));
    zip.add(
        "xl/drawings/_rels/drawing1.xml.rels",
        xmlBytes([&](QXmlStreamWriter &w) {
          w.writeStartElement("Relationships");
          w.writeDefaultNamespace(
              "http://schemas.openxmlformats.org/package/2006/relationships");
          for (int i = 0; i < imageCount; ++i) {
            w.writeEmptyElement("Relationship");
            w.writeAttribute("Id", "rId" + QString::number(i + 1));
            w.writeAttribute("Type", "http://schemas.openxmlformats.org/"
                                     "officeDocument/2006/relationships/image");
            w.writeAttribute("Target", "../media/image" +
                                           QString::number(i + 1) + ".png");
          }
          w.writeEndElement();
        }));
    zip.add(
        "xl/worksheets/_rels/sheet" + QString::number(graphSheet) + ".xml.rels",
        R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing" Target="../drawings/drawing1.xml"/></Relationships>)");
  }
  zip.add("xl/workbook.xml", xmlBytes([&](QXmlStreamWriter &w) {
            w.writeStartElement("workbook");
            w.writeDefaultNamespace(ns);
            w.writeNamespace("http://schemas.openxmlformats.org/officeDocument/"
                             "2006/relationships",
                             "r");
            w.writeStartElement("sheets");
            for (int i = 0; i < sheetNames.size(); ++i) {
              w.writeEmptyElement("sheet");
              w.writeAttribute("name", sheetNames[i]);
              w.writeAttribute("sheetId", QString::number(i + 1));
              w.writeAttribute("r:id", "rId" + QString::number(i + 1));
            }
            w.writeEndElement();
            w.writeEndElement();
          }));
  zip.add("xl/_rels/workbook.xml.rels", xmlBytes([&](QXmlStreamWriter &w) {
            w.writeStartElement("Relationships");
            w.writeDefaultNamespace(
                "http://schemas.openxmlformats.org/package/2006/relationships");
            for (int i = 0; i < sheetNames.size(); ++i) {
              w.writeEmptyElement("Relationship");
              w.writeAttribute("Id", "rId" + QString::number(i + 1));
              w.writeAttribute("Type",
                               "http://schemas.openxmlformats.org/"
                               "officeDocument/2006/relationships/worksheet");
              w.writeAttribute("Target", "worksheets/sheet" +
                                             QString::number(i + 1) + ".xml");
            }
            w.writeEmptyElement("Relationship");
            w.writeAttribute("Id", "rIdStyles");
            w.writeAttribute("Type",
                             "http://schemas.openxmlformats.org/officeDocument/"
                             "2006/relationships/styles");
            w.writeAttribute("Target", "styles.xml");
            w.writeEndElement();
          }));
  zip.add(
      "_rels/.rels",
      R"(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)");
  zip.add(
      "xl/styles.xml",
      R"(<?xml version="1.0"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="11"/><color rgb="FFFFFFFF"/><name val="Calibri"/></font></fonts><fills count="5"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FF172E47"/><bgColor indexed="64"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="FFD8F1E7"/><bgColor indexed="64"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="FFFFDDE0"/><bgColor indexed="64"/></patternFill></fill></fills><borders count="1"><border/></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="4"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/><xf numFmtId="0" fontId="1" fillId="2" borderId="0" xfId="0" applyFill="1" applyFont="1"/><xf numFmtId="0" fontId="0" fillId="3" borderId="0" xfId="0" applyFill="1"/><xf numFmtId="0" fontId="0" fillId="4" borderId="0" xfId="0" applyFill="1"/></cellXfs><cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>)");
  zip.add(
      "[Content_Types].xml", xmlBytes([&](QXmlStreamWriter &w) {
        w.writeStartElement("Types");
        w.writeDefaultNamespace(
            "http://schemas.openxmlformats.org/package/2006/content-types");
        for (auto pair :
             {std::pair{
                  "rels",
                  "application/vnd.openxmlformats-package.relationships+xml"},
              std::pair{"xml", "application/xml"},
              std::pair{"png", "image/png"}}) {
          w.writeEmptyElement("Default");
          w.writeAttribute("Extension", pair.first);
          w.writeAttribute("ContentType", pair.second);
        }
        auto add = [&](QString name, QString type) {
          w.writeEmptyElement("Override");
          w.writeAttribute("PartName", name);
          w.writeAttribute("ContentType", type);
        };
        add("/xl/workbook.xml",
            "application/"
            "vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
        add("/xl/styles.xml",
            "application/"
            "vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
        for (int i = 0; i < sheetNames.size(); ++i)
          add("/xl/worksheets/sheet" + QString::number(i + 1) + ".xml",
              "application/"
              "vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
        if (imageCount)
          add("/xl/drawings/drawing1.xml",
              "application/vnd.openxmlformats-officedocument.drawing+xml");
        w.writeEndElement();
      }));
  if (control)
    control->check();
  zip.finish();
}
