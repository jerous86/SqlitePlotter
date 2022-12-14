#pragma once

#include <QString>
#include <QMap>
#include <QVariant>
#include <QIODevice>

struct PlotOptions;
struct Trace;

enum ETraceStyle {
	Line,
	Points2D,
	Points3D,
	Histogram2D,
	Histogram3D,
	Impulse,
	Step,
	Box,
	VertHistogram,
	Violin,
    StackedLine,
};

class QPainter;
struct MyPlotterState;
struct ObjectsList;
class QCPAbstractPlottable;
class MyQCustomPlotter;
struct Plot;

struct TraceStyle {
	virtual ~TraceStyle() { }
	// The numbers of dimensions this type supports
	// should return any |-combination of (1<<i), where i \in {1,2,3}
	virtual ETraceStyle style() const=0;
	virtual QString name() const=0;
	virtual TraceStyle *clone() const=0;

	bool supports_dim(int i) const { return dim() & (1<<i); }
	virtual QString help() const=0;
	virtual QString toString() const;

	// Choose between plot or splot (for 3d).
	virtual QString plot_type() const { return "plot"; }

	QByteArray serialize() const;
	static TraceStyle *deserialize(QByteArray &data);

	QMap<QString,QVariant> options;
	QVariant get_opt(const Trace &trace, const QString &name, const QVariant &default_) const;
	double get_opt_dbl(const Trace &trace, const QString &name, const double &default_) const;
	bool get_opt_bool(const Trace &trace, const QString &name, const bool &default_) const;
	QString get_opt_str(const Trace &trace, const QString &name, const QString &default_) const;

	// Returns a list of extra column names this style needs.
	virtual QStringList extra_columns(const Trace &) const { return QStringList(); }

    virtual QCPAbstractPlottable *plot_QCustomPlot(const Plot &plot, MyQCustomPlotter *p,
            const Trace &trace, int serieIdx,
            const QVector<double> &x, const QVector<double> &y, const QVector<double> &z,
            QVector<QCPAbstractPlottable *> &graphs) {
        (void) plot;
        (void) p;
        (void) trace;
        (void) serieIdx;
        (void) x; (void) y; (void) z;
        (void) graphs;
        return nullptr;
    }

	virtual void plot(QPainter &p, const MyPlotterState &state, int serieIdx, ObjectsList &objList) { (void) p; (void) state; (void) serieIdx; (void) objList; }
	virtual void handleMouseClick(MyPlotterState &state, int serieIdx, const QVector2D &mouse, QVector3D &closest, unsigned &bestDst) { (void) serieIdx; (void) state; (void) mouse; (void) closest; (void) bestDst;  }
protected:
	virtual int dim() const=0;
	virtual void no_vtable_warn()=0;

    QString add_line_helper(const PlotOptions &opts, const Trace &trace, const QString &columns, const QString &extra_title, const QString &options, const QStringList &col_names, const QList<QList<QVariant> > &rows) const;
    QString add_titleless_line_helper(const PlotOptions &opts, const Trace &trace, const QString &columns, const QString &options) const;
};

TraceStyle *name_to_traceStyle(const QString &name);

enum NormalizeMode {
	// Just use the counts
	DoNotNormalize=0,
	// The sum of frequencies is 1
	NormalizeSumTo1=1,
	// The maximum value is scaled to 1
	NormalizeToMax=2,
};

QList<QPointF> freq_table(const QList<double> &rows, const size_t n_bins, const NormalizeMode normalize);
QList<QPointF> freq_table(const QList<double> &rows, const size_t n_bins, double min, double max, const NormalizeMode normalize);

struct ViolinPlotStats {
	double mean, median;
	double mediangt0; // median when ignoring values<0
	double q1, q3;
	double p5, p95;
	QList<double> xs;
	QList<double> ps;
};
ViolinPlotStats violinplot_stats(const QList<double> &rows, const size_t divisions, double h, int kernel_i);
