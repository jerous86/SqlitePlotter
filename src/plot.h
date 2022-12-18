#pragma once

#include <QString>
#include <QList>
#include <QDebug>
#include <QSettings>

#include "db.h"
#include "tracestyles.h"

struct Trace;
struct Plot;
struct TraceStyle;
class MainWindow;

typedef QMap<QString,QString> MapQStringQString;
extern QList<TraceStyle *> traceStyles;

MapQStringQString merge(const MapQStringQString &vars, const MapQStringQString &override);
bool isSpecialQuery(const QString &q);

struct Set {
	Set(Db &db_);
	Set(const Set &)=delete;
	Set &operator=(const Set &set_);
	virtual ~Set();
	Set *clone() const;

	Db &db;
	QList<Plot *> plots;

	bool autoReplot;
    enum PlotMode {
        QCustomPlot=0,
        MyPlotter=1,
	};
	int plotMode;
	void setPlotMode(int plotMode_);
	QString imageSize;
	bool stableColors;
	bool bbox;
	MapQStringQString globalVars;

	void store(QSettings &settings) const;
	void restore(QSettings &settings);
};

struct Plot {
	Plot(Set &set_): set(&set_) { }
	virtual ~Plot();
	Plot *clone() const;

	QString get_query(const MapQStringQString &override_vars) const;
    QString getTableName() const;
	QString get_title(const MapQStringQString &override_vars) const;
	QString toString(const MapQStringQString &override_vars) const;
	QString get_xAxisLabel(const MapQStringQString &override_vars) const;
	QString get_yAxisLabel(const MapQStringQString &override_vars) const;
	QString get_zAxisLabel(const MapQStringQString &override_vars) const;

	// If all=false, we return the index within the enabled traces
	int traceIdx(const Trace *, bool all=true) const;
	int traceIdx(const Trace &trace, bool all=true) const { return traceIdx(&trace, all); }
    int allIdxToEnabledIdx(int allIdx) const;

	// Either plot or splot
	QString plot_type() const;

	QList<Trace *> traces;
	Set *set;

	QString defaultQuery;
	QString title;
	bool logX, logY, logZ;
	QString xAxisLabel, yAxisLabel, zAxisLabel;
	QString xmin,xmax, ymin,ymax, zmin,zmax;
	int altColumns, altRows;
	MapQStringQString variables;
	bool expanded;

	bool d3Grid;
	int d3Xrot, d3Zrot;
	int d3Mesh;

	void store(QSettings &settings) const;
	void restore(QSettings &settings);
	void apply(MainWindow &mw) const;
	QList<int> pos() const;
	QString apply_variables(const QString &str, const MapQStringQString &override_vars) const;
};

enum UseTraceQuery {
	DoUseTraceQuery=1,
	DoNotUseTraceQuery=2,
};
enum UngroupQuery {
	Ungroup=1,
	NoUngroup=2,
};
struct Trace {
	Trace(Plot &plot_);
	~Trace();
	Trace *clone() const;

	void store(QSettings &settings) const;
    void restore(QSettings &settings);
	QList<int> pos() const;
	QString apply_variables(const QString &str, const MapQStringQString &override_vars) const;

	QString get_query(const MapQStringQString &override_vars) const;
    QString getTableName() const;
	QString get_title(const MapQStringQString &override_vars) const;
	QString toString(const MapQStringQString &override_vars) const;
	int dim() const;

	Plot *plot=nullptr;
	QString query="query not set";
	QString x="", y="", z="";
	bool xtic=false, ytic=false, ztic=false;
	//	QString type;
	TraceStyle *style=nullptr;
	QString title="(title not set)";
	bool enabled=false;
	MapQStringQString variables;

	QStringList column_names(UseTraceQuery use_trace_query, enum UngroupQuery ungroup, const MapQStringQString &override_vars) const;
	int column_index(const QString &col, const MapQStringQString &override_vars) const;

	typedef QList<QVariant> Row;
	typedef QList<Row> Rows;
	const Rows *data(const QStringList &select_columns, UseTraceQuery use_trace_query, enum UngroupQuery ungroup, const MapQStringQString &override_vars) const;
	unsigned rowCount(UseTraceQuery use_trace_query, enum UngroupQuery ungroup, const MapQStringQString &override_vars) const;

	static void invalidateCache();

private:
	static QList<QPair<QString,Rows>> mruCache;
	const Rows *findInCache(const QString &id) const;
	const Rows *addToCache(const QString &id, const Rows &rows) const;
	void printCache(const QString &title) const;
};

