void MainWindow::refreshResults() {
  QString sort = rankBox->currentIndex() == 0 ? "Margin" : rankBox->currentIndex() == 1 ? "Value" : rankBox->currentIndex() == 2 ? "Channel" : "Result";
  si::rankResults(results, ss(sort)); resultRows.clear();
  const auto activeRevision = activeRevisionName();
  for (size_t i=0;i<results.size();++i)
    if (results[i].revision == activeRevision &&
        (directionMode==2 || results[i].reverse==(directionMode==1)))
      resultRows.push_back(i);
  QSignalBlocker block(table); table->setRowCount(int(resultRows.size())); std::vector<std::string> statuses; int ng=0,na=0,ok=0;
  for (size_t rowIndex=0; rowIndex<resultRows.size(); ++rowIndex) {
    auto &r=results[resultRows[rowIndex]]; statuses.push_back(r.status); if(r.status=="NG")++ng; else if(r.status=="OK")++ok; else ++na;
    QStringList row{qs(r.channel),qs(r.revision),qs(si::name(r.metric)),qs(r.parameter),num(r.worst),num(r.worstX*(r.metric==si::Metric::TDR?1e9:1e-9),4),num(r.margin),(r.metric==si::Metric::RL||r.metric==si::Metric::IL)?num(r.directionDelta):"",(r.metric==si::Metric::RL||r.metric==si::Metric::IL)?num(r.directionDeltaX*1e-9,4):"",qs(r.status),qs(r.aggressor),qs(r.quality),r.metric==si::Metric::TDR?qs(si::name(r.termination,r.parameter.starts_with("Sdd")))+(r.reflection?" / rho":" / ohm"):"dB"};
    for(int j=0;j<row.size();++j){auto item=new QTableWidgetItem(row[j]);if(r.status=="NG")item->setBackground(QColor("#fff0f1"));if(j==9)item->setForeground(r.status=="NG"?QColor("#c6374a"):r.status=="OK"?QColor("#00856c"):QColor("#6d7f94"));table->setItem(int(rowIndex),j,item);}
  }
  refreshResultLanguage();
  table->resizeColumnsToContents(); table->setColumnWidth(3,160); sparamview::ui_language::message(summary,"Overall %1   ·   OK %2   NG %3   N/A %4",{qs(si::overall(statuses)),QString::number(ok),QString::number(ng),QString::number(na)});
}
// Re-render presentation in place: no sorting, row reconstruction, selection
// changes, analysis generation changes or changes to the canonical Result.
void MainWindow::refreshResultLanguage() {
  const QSignalBlocker block(table);
  const auto language=sparamview::ui_language::current(this);
  for(int rowIndex=0;rowIndex<table->rowCount() && size_t(rowIndex)<resultRows.size();++rowIndex) {
    const auto index=resultRows[size_t(rowIndex)];
    if(index>=results.size()) continue;
    const auto &r=results[index];
    auto tip=sparamview::ui_language::diagnosticText(qs(r.note),language)+"\n"+
        sparamview::ui_language::menuText("Worst margin at ",language)+
        num(r.marginX*(r.metric==si::Metric::TDR?1e9:1e-9),6)+
        (r.metric==si::Metric::TDR?" ns":" GHz")+"\n"+
        sparamview::ui_language::menuText(qs(r.direction),language);
    if((r.metric==si::Metric::RL||r.metric==si::Metric::IL)&&!std::isnan(r.directionDelta))
      tip+="\n"+sparamview::ui_language::menuText("Max directional Δ: ",language)+num(r.directionDelta,4)+" dB @ "+num(r.directionDeltaX*1e-9,6)+" GHz";
    if(auto *item=table->item(rowIndex,2))
      item->setText(sparamview::ui_language::menuText(qs(si::name(r.metric)),language));
    if(r.metric==si::Metric::TDR)
      if(auto *item=table->item(rowIndex,12))
        item->setText(sparamview::ui_language::menuText(qs(si::name(r.termination,r.parameter.starts_with("Sdd"))),language)+(r.reflection?" / rho":" / ohm"));
    for(int col=0;col<table->columnCount();++col)
      if(auto *item=table->item(rowIndex,col)) item->setToolTip(tip);
  }
}
void MainWindow::showMetric(si::Metric m) {
  displayed = m;
  { QSignalBlocker block(metricView); metricView->setCurrentIndex(int(m)); }
  for (int i=0;i<5;++i) metricButtons[i]->setChecked(i==int(m));
  plot->resetView();
  refreshPlot();
  tabs->setCurrentIndex(0);
}
const si::Job *MainWindow::jobFor(const si::Result &r) const {
  for (auto &j : lastJobs) {
    const bool reverse = j.reverse.value_or(lastSettings.reverse);
    if (j.metric == r.metric && j.termination == r.termination && j.victim.id == r.channelId && j.revision == r.revision && reverse == r.reverse && (j.aggressor ? j.aggressor->id : "") == r.aggressorId) return &j;
  }
  return nullptr;
}
