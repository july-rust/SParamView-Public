std::string name(Metric m) {
  switch (m) {
  case Metric::RL:
    return "Return Loss";
  case Metric::IL:
    return "Insertion Loss";
  case Metric::NEXT:
    return "NEXT";
  case Metric::FEXT:
    return "FEXT";
  case Metric::TDR:
    return "TDR";
  }
  return {};
}
const Limit &Settings::limit(Metric m) const {
  switch (m) {
  case Metric::RL:
    return rl;
  case Metric::IL:
    return il;
  case Metric::NEXT:
    return next;
  case Metric::FEXT:
    return fext;
  default:
    return rl;
  }
}
void Limit::validate() const {
  if (!std::isfinite(constant))
    throw Error("Invalid constant limit");
  double last = -1;
  for (auto [f, v] : points) {
    if (!std::isfinite(f) || !std::isfinite(v) || f < 0 || f <= last)
      throw Error("Limit frequency points must be finite and increasing");
    last = f;
  }
  if (points.size() == 1)
    throw Error("Frequency limit needs at least two points");
}
double Limit::at(double f) const {
  if (!enabled)
    return NaN;
  if (points.empty())
    return constant;
  if (f < points.front().first || f > points.back().first)
    return NaN;
  auto it = std::lower_bound(points.begin(), points.end(), f,
                             [](auto p, double x) { return p.first < x; });
  if (it == points.begin() || it->first == f)
    return it->second;
  auto a = *(it - 1), b = *it;
  return a.second + (b.second - a.second) * (f - a.first) / (b.first - a.first);
}
static std::vector<std::pair<int, double>> endpoint(const Channel &c,
                                                    bool far) {
  int p = far ? c.farP : c.nearP, n = far ? c.farN : c.nearN;
  if (p < 0)
    throw Error("Far end is not mapped: " + c.name);
  if (n < 0)
    return {{p, 1}};
  return {{p, 1 / std::sqrt(2.)}, {n, -1 / std::sqrt(2.)}};
}

std::string name(Termination t, bool diff) {
  switch(t) {
  case Termination::Reference:return "Reference matched (all physical ports)";
  case Termination::Resistor:return diff?"P-N 100 ohm (floating)":"Signal-GND 50 ohm";
  case Termination::Split:return diff?"P-GND 50 ohm + N-GND 50 ohm":"Signal-GND 50 ohm";
  case Termination::Open:return diff?"P and N open":"Signal open";
  case Termination::Short:return diff?"P-N short (floating)":"Signal-GND short";
  } throw Error("Invalid termination");
}
std::string shortName(Termination t,bool diff) {
  switch(t) {
  case Termination::Reference:return "REF";
  case Termination::Resistor:return diff?"PN100":"50G";
  case Termination::Split:return diff?"GND50x2":"50G";
  case Termination::Open:return "OPEN";
  case Termination::Short:return "SHORT";
  } throw Error("Invalid termination");
}
std::vector<Job> expandTdrJobs(const std::vector<Job> &jobs,const Settings &s) {
  std::vector<Job> out;
  for(const auto &job:jobs) {
    if(job.metric!=Metric::TDR) {out.push_back(job);continue;}
    if(s.tdrTerminations.empty()) throw Error("Select at least one TDR termination");
    std::vector<Termination> seen;
    for(auto t:s.tdrTerminations) {
      (void)name(t,job.victim.differential());
      if(!job.victim.differential()&&t==Termination::Split)t=Termination::Resistor;
      if(std::find(seen.begin(),seen.end(),t)!=seen.end())continue;
      seen.push_back(t);auto copy=job;copy.termination=t;out.push_back(copy);
    }
  } return out;
}
// A singular floating mode is acceptable only when the physical load
// boundary is consistent AND the measured input response is unique.
static Complex loadFeedback(int n,Complex a[2][2],const Complex b[2],const Complex response[2]) {
  Complex m[2][3]{},original[2][2]{};double scale=1;
  for(int i=0;i<n;++i) {
    m[i][n]=b[i];
    for(int j=0;j<n;++j) {original[i][j]=m[i][j]=a[i][j];scale=std::max(scale,std::abs(a[i][j]));}
  }
  int pivots[2]{},rank=0;
  for(int col=0;col<n&&rank<n;++col) {
    int best=rank;
    for(int i=rank;i<n;++i)if(std::abs(m[i][col])>std::abs(m[best][col]))best=i;
    if(std::abs(m[best][col])<=1e-12*scale)continue;
    for(int j=0;j<=n;++j)std::swap(m[rank][j],m[best][j]);
    auto pivot=m[rank][col];for(int j=0;j<=n;++j)m[rank][j]/=pivot;
    for(int i=0;i<n;++i)if(i!=rank){auto f=m[i][col];for(int j=0;j<=n;++j)m[i][j]-=f*m[rank][j];}
    pivots[rank++]=col;
  }
  for(int i=rank;i<n;++i)if(std::abs(m[i][n])>1e-10)throw Error("Load boundary is singular/inconsistent");
  Complex x[2]{};
  for(int i=0;i<rank;++i)x[pivots[i]]=m[i][n];
  for(int col=0;col<n;++col) {
    bool pivot=false;for(int i=0;i<rank;++i)pivot|=pivots[i]==col;
    if(pivot)continue;
    Complex effect=response[col];
    for(int i=0;i<rank;++i)effect-=response[pivots[i]]*m[i][col];
    if(std::abs(effect)>1e-10)throw Error("Load boundary has no unique input response");
  }
  Complex out{};
  for(int i=0;i<n;++i) {
    Complex residual=-b[i];for(int j=0;j<n;++j)residual+=original[i][j]*x[j];
    if(std::abs(residual)>1e-8*(1+std::abs(b[i])))throw Error("Load boundary residual exceeds tolerance");
    out+=response[i]*x[i];
  }
  if(!std::isfinite(out.real())||!std::isfinite(out.imag()))throw Error("Non-finite loaded response");
  return out;
}
static void terminateTrace(Trace &t,const Cache &cache,const Channel &ch,const Settings &set,Termination term,Control *control) {
  (void)name(term,ch.differential());t.termination=term;
  if(term==Termination::Reference)return;
  auto driven=endpoint(ch,set.reverse),loaded=endpoint(ch,!set.reverse);
  const int n=int(loaded.size());
  if(ch.differential()&&n!=2)throw Error("Both opposite-end P/N ports must be mapped for differential loading");
  Complex gamma[2][2]{};double z[2]{};
  for(int i=0;i<n;++i)z[i]=cache.meta().reference.at(size_t(loaded[i].first));
  if(term==Termination::Open){for(int i=0;i<n;++i)gamma[i][i]=1;}
  else if(n==2&&(term==Termination::Resistor||term==Termination::Short)) {
    // Floating P-N resistor: Y = [1,-1]^T [1,-1] / R.
    double denom=(term==Termination::Resistor?100.:0.)+z[0]+z[1];
    double v[2]{std::sqrt(z[0]),-std::sqrt(z[1])};
    for(int i=0;i<2;++i)for(int j=0;j<2;++j)gamma[i][j]=(i==j?1.:0.)-2*v[i]*v[j]/denom;
  } else {
    double r=term==Termination::Short?0.:50.;
    for(int i=0;i<n;++i)gamma[i][i]=(r-z[i])/(r+z[i]);
  }
  std::vector<Complex> ll[2][2],ln[2],nl[2];
  for(int i=0;i<n;++i) {
    auto physical=std::vector<std::pair<int,double>>{{loaded[i].first,1}};
    ln[i]=cache.trace(physical,driven,control);nl[i]=cache.trace(driven,physical,control);
    for(int j=0;j<n;++j)ll[i][j]=cache.trace(physical,{{loaded[j].first,1}},control);
  }
  for(size_t k=0;k<t.s.size();++k) {
    if(k%128==0)check(control);
    Complex a[2][2]{},b[2]{},response[2]{};
    for(int i=0;i<n;++i) {
      response[i]=nl[i][k];
      for(int j=0;j<n;++j) {
        b[i]+=gamma[i][j]*ln[j][k];a[i][j]=(i==j?1.:0.);
        for(int q=0;q<n;++q)a[i][j]-=gamma[i][q]*ll[q][j][k];
      }
    }
    t.s[k]+=loadFeedback(n,a,b,response);
  }
  t.parameter+=" (loaded)";
}
