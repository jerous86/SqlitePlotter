#include "tracestyles.h"

#include <cmath>

#include <QRegularExpression>

#include "plot.h"
#include "misc.h"
#include "plot_task.h"
#include "tracestyle/helpers.h"

const MapQStringQString EmptyOverride=MapQStringQString();

QString _using2(const Trace &trace, QString x, QString y, QString use_z, QStringList extraCols, double x_offset, double y_offset) {
	QStringList pts;
	if (true) { const QString col=QString(trace.xtic? "($2+%1)" : "($3+%1)").arg(x_offset); pts<<x.replace("${COL}", col); }
	if (true) { const QString col=QString(trace.ytic? "($4+%1)" : "($5+%1)").arg(y_offset); pts<<y.replace("${COL}", col); }
	if (use_z.length()) { pts<<(trace.ztic? "($6)" : use_z.replace("${COL}","($7)")); }
	pts<<extraCols;
	return pts.join(":");
}
QString _using(const Trace &trace, bool use_x, bool use_y, QString use_z, double x_offset, double y_offset) {
	QStringList pts;
	if (use_x) { pts<<QString(trace.xtic? "($2+%1)" : "($3+%1)").arg(x_offset); }
	if (use_y) { pts<<QString(trace.ytic? "($4+%1)" : "($5+%1)").arg(y_offset); }
	if (use_z.length()) { pts<<(trace.ztic? "($6)" : use_z.replace("${COL}","($7)")); }
	return pts.join(":");
}

QString apply_fs(QString str, const QStringList &col_names, const QList<QList<QVariant>> &rows) {
	if (rows.size()>0) {
		const auto i=col_names.indexOf("TG");
		if (i>0 && i<rows[0].size()) {
			const QString repl=rows[0][i].toString();
			str.replace("${COL:TG}", repl);
		}
	}
	return str;
}


QList<TraceStyle *> traceStyles;
void deinit_trace_styles() {
	qDeleteAll(traceStyles);
	traceStyles.clear();
}

QString TraceStyle::add_line_helper(const PlotOptions &opts, const Trace &trace, const QString &columns, const QString &extra_title, const QString &options,
				    const QStringList &col_names, const QList<QList<QVariant>> &rows
				    ) const {
	const Plot &plot=*trace.plot;
	const int i=plot.traceIdx(trace);

	QString summary;
	if (trace.dim()==2) {
		summary=QString("sprintf('Y:[%.3f->%.3f->%.3f]', min_%1_1, avg_%1_1, max_%1_1)").arg(i);
	} else if (trace.dim()==3) {
		summary=QString("sprintf('Y:[%.3f->%.3f->%.3f] Z:[%.3f->%.3f->%.3f]', min_%1_1, avg_%1_1, max_%1_1, min_%1_2, avg_%1_2, max_%1_2)").arg(i);
	} else { DEBUG(trace.dim()); }

	if (opts.plotLegend==false) { return add_titleless_line_helper(opts, trace, columns, options); }

	const QString title=plot.apply_variables(apply_fs(trace.get_title(EmptyOverride),col_names,rows), trace.variables);
	if (title=="-") {
		return QString("$DATA_%1 using %2 title '' with %3")
				.arg(i) // 1
				.arg(columns) // 2
				.arg(options); // 3
	} else {
		return QString("$DATA_%1 using %2 title sprintf('%s %s','%3',%4) with %5")
				.arg(i) // 1
				.arg(columns) // 2
				.arg(title) // 3
				.arg(extra_title.length()  ? extra_title : summary) // 4
				.arg(options); // 5
	}
}

QString TraceStyle::add_titleless_line_helper(const PlotOptions &opts, const Trace &trace, const QString &columns, const QString &options) const {
	(void) opts;
	const Plot &plot=*trace.plot;
	const int i=plot.traceIdx(trace);
	return QString("$DATA_%1 using %2 title '' with %3") .arg(i).arg(columns,options);
}

QString TraceStyle::toString() const {
	QStringList pts;
	for(auto &x:options.keys()) { pts<<x+"="+options[x].toString(); }
	return options.size()  ? (name()+": "+pts.join("; "))  : name();
}

QByteArray TraceStyle::serialize() const {
	QByteArray data;
	QDataStream stream(&data, QIODevice::WriteOnly);
	stream.writeBytes(name().toLatin1().data(), uint(name().length()));
	stream<<options;
	return data;
}

TraceStyle *TraceStyle::deserialize(QByteArray &data) {
	QDataStream stream(&data, QIODevice::ReadOnly);
	char *name=new char[20];
	uint len=0;
	stream.readBytes(name, len);
	TraceStyle *style=name_to_traceStyle(name);
	if (style) { stream>>style->options; } else if (QString(name).length()) { DEBUG("Didn't find style '"<<name<<"' "<<len); }
	delete []name;
	return style;
}

QVariant TraceStyle::get_opt(const Trace &trace, const QString &name, const QVariant &default_) const {
	const QVariant v=options.contains(name) ? options[name] : default_;
	if (v.userType()==QMetaType::QString) {
		QVariant str_to_qvariant(const QString &txt);
		return str_to_qvariant(trace.apply_variables(v.toString() , MapQStringQString()));
	} else { return v;
	}
}

double TraceStyle::get_opt_dbl(const Trace &trace, const QString &name, const double &default_) const {
	return get_opt(trace, name, default_).toDouble();
}

bool TraceStyle::get_opt_bool(const Trace &trace, const QString &name, const bool &default_) const {
	const QString v=get_opt(trace, name, default_).toString().toLower();
	if (v=="true" || v=="t") { return true;
	} else if (v=="false" || v=="f") { return false;
	} else { return int(v.toDouble())!=0;
	}
}

QString TraceStyle::get_opt_str(const Trace &trace, const QString &name, const QString &default_) const {
	return get_opt(trace, name, default_).toString();
}


TraceStyle *name_to_traceStyle(const QString &name) {
	for(auto *style:traceStyles) { if (style->name()==name) { return style->clone(); } }
	return nullptr;
}

struct Impulse: public TraceStyle {
	HELPER_2D(Impulse)
	QString help() const override { return "Draws impulses."; }
};
void Impulse::no_vtable_warn() { }

struct Step: public TraceStyle {
	HELPER_2D(Step)
	QString help() const override { return "Draws step functions."
					       "\nOption: mode [=steps; string]: Must be one of {steps,fsteps,histeps}."; }
};
void Step::no_vtable_warn() { }

struct Box: public TraceStyle {
	HELPER_2D(Box)
	QString help() const override { return "Draws boxes."; }
};
void Box::no_vtable_warn() { }


QList<QPointF> freq_table(const QList<double> &rows, const size_t n_bins, const NormalizeMode normalize) {
	// First determine min and max
	const size_t N=size_t(rows.size());
	if (N==0) { return QList<QPointF>(); }
	double min=rows[0],max=rows[0];
	for(size_t i=1; i<N; i++) {
		const double v=rows[int(i)];
		if (!std::isfinite(v)) { continue; }
		min=std::min(min,v);
		max=std::max(max,v);
	}
	if (min>=max) { return QList<QPointF>(); }
	return freq_table(rows, n_bins, min, max, normalize);
}

bool eq(double l, double r) { return fabs(double(l)-double(r))<=1e-10; }

QList<QPointF> freq_table(const QList<double> &rows, size_t n_bins, double min, double max, const NormalizeMode normalize) {
	Q_ASSERT(min<max);
	const size_t N=size_t(rows.size());
	const double bin_size=(max-min)/n_bins;

	QVector<double> freqs;
	freqs.resize(size_t(n_bins));
	size_t max_f=0;
	for(size_t i=0; i<N; i++) {
		const double v=rows[int(i)];
		if (!std::isfinite(v)) { continue; }

		Q_ASSERT(v>=min);
		Q_ASSERT(v<=max);
		const size_t bin=eq(v,max) ? size_t(n_bins-1) : size_t((v-min)/bin_size);
//		Q_ASSERT(bin>=0);
		Q_ASSERT(bin<size_t(n_bins));
		freqs[bin]+=1;
		max_f=std::max(size_t(freqs[bin]), max_f);
	}

	if (normalize==NormalizeSumTo1) {
		for(auto &x:freqs) { x/=N; }
	} else if (normalize==NormalizeToMax) {
		for(auto &x:freqs) { x/=max_f; }
	}

	QList<QPointF> ret;
	for(size_t i=0; i<size_t(n_bins); i++) {
		ret<<QPointF(min+i*bin_size, freqs[i]);
	}

	return ret;
}

#include <numeric>
template<class T=double>
struct OnlineStats {
	virtual ~OnlineStats() { }
	explicit OnlineStats(): n(0), mean(T(0)), variance(T(0)), sum(T(0)) { }
	explicit OnlineStats(const std::vector<T> &vx): OnlineStats() { for(auto x : vx) { collect(x); } }

	void reset() {
		n=0;
		mean=variance=min=max=sum=T(0);
		weight_sum=0;
	}
	uint64_t getN() const { return n; }
	T getMean() const { Q_ASSERT(n>0); return mean; }
	T getVariance() const { return T(n>1 ? variance/T(n-1) : 0.); }
	T getStdDev() const { return T(sqrt(double(getVariance()))); }

	T getMin() const { return min; }
	T getMax() const { return max; }
	T getSum() const { return sum; }

	template<class T2> void collect(const T2 &x) { _collect(x, 1.); }

	double outputMultiplier=1;

private:
	// See also http://people.ds.cam.ac.uk/fanf2/hermes/doc/antiforgery/stats.pdf (93,113)
	void _collect(const T x, const double weight) {
		n++;
		const double old_wsum=weight_sum;
		weight_sum+=weight;

		const double delta=(x)-(mean);
		const double incr=(weight/weight_sum) * delta; // (93)
		mean = mean + T(incr); // (93)
		variance = T(
					(old_wsum>0 ? (weight_sum-weight)/old_wsum * (variance) : 0) +
					(weight * /*old delta*/delta * /*new delta*/((x)-(mean)))
					); // (113)
		sum+=x;

		if (n==1) { min=max=x; } else { min=std::min(x,min); max=std::max(x,max); }
	}
	uint64_t n=0;
	T mean=T(0);
	T variance=T(0);
	T min=T(0);
	T max=T(0);
	T sum=T(0);
	double weight_sum=0;
};

// https://gist.github.com/cboettig/549857
/** Estimate bandwidth using Silverman's "rule of thumb"
 * (Silverman 1986, pg 48 eq 3.31).  This is the default
 * bandwith estimator for the R 'density' function.  */
double nrd0(const QList<double> &xs) {
	OnlineStats<double> stats;
	for(auto &x:xs) { stats.collect(x); }

	Q_ASSERT(std::is_sorted(xs.begin(), xs.end()));

	const int N=int(xs.size());
	double hi = stats.getStdDev();
	double iqr = xs[int(N*3./4.)] - xs[int(N*1./4.)]; // assumes data is sorted!
	double lo = std::min(hi, iqr/1.34);
	double bw = 0.9 * lo * pow(N, -0.2);
	return bw;
}

double gauss_kernel(double x) {  return exp(-(std::pow(x,2)/2))/(M_SQRT2*sqrt(M_PI));  }
double epanechnikov_kernel(double x) { return fabs(x)<=1 ? 3./4.*(1-std::pow(x,2)) : 0; }

double kerneldensity(const double h, const QList<double> &samples, double obs, int kernel_i) {
	double prob = 0;
	const auto n=samples.size();
	double (* kernels [])(double) = {gauss_kernel, epanechnikov_kernel};
	if (kernel_i<0 || kernel_i>1) { kernel_i=0; }
	auto kernel=kernels[kernel_i];
	for(auto s:samples) {
		const double x=(obs-s)/h;
		const double v=1./(n*h) * kernel(x);
		prob += v;
	}
	return prob;
}

ViolinPlotStats violinplot_stats(const QList<double> &rows, const size_t divisions, double h_, int kernel_i) {
	ViolinPlotStats ret;

	QList<double> v(rows);
	const auto N=v.size();
	std::sort(v.begin(), v.end());

	int gt0Offset=0;
	while (gt0Offset<v.size() && v[gt0Offset]<=0) { ++gt0Offset; }
	const int mediangt0_i=int(gt0Offset+(N-gt0Offset) * (1./2.));

	ret.mean=std::accumulate(v.begin(), v.end(), 0.0)/N;
	ret.median=v[int(N * (1./2.))];
	ret.mediangt0=mediangt0_i<v.size() ? v[mediangt0_i] : nan("");
	ret.q1=v[int(N * (1./4.))];
	ret.q3=v[int(N * (3./4.))];
	ret.p5=v[int(N * (5./100.))];
	ret.p95=v[int(N * (95./100.))];

	const double h = h_<=1e-7
		? std::max(1e-6, nrd0(v))
		: h_;
	const auto step=(v[N-1]-v[0])/divisions;
	double p_sum=0, p_max=0;
	for(auto x=v[0]; x<v[N-1]; x+=step) {
		ret.xs<<x;
		ret.ps<<kerneldensity(h, v, x, kernel_i);
		p_sum+=ret.ps.back();
		p_max=std::max(p_max, ret.ps.back());
	}
    (void) p_sum;
	for(auto &x:ret.ps) { x/=p_max; }

	return ret;
}

TraceStyle *newHistogram2D();
TraceStyle *newHistogram3D();
TraceStyle *newPoints2D();
TraceStyle *newPoints3D();
TraceStyle *newViolin();
TraceStyle *newVertHistogram();
TraceStyle *newStackedLine();


void init_trace_styles() {
	traceStyles
		<<newPoints2D()
		<<newPoints3D()
//		<<newHistogram2D()
//		<<newHistogram3D()
		<<new struct Impulse()
		<<new struct Step()
		<<new struct Box()
//		<<newVertHistogram()
//        <<newViolin()
        <<newStackedLine()
        ;
}
