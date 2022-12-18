#include "plot.h"

#include <QRegularExpression>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "misc.h"

#define PLOT_CLONE_VAR(VAR)                         ret->VAR=VAR
#define PLOT_STORE_SETTING(VAR)                     settings.setValue(#VAR, VAR)
#define PLOT_RESTORE_SETTING_STR(VAR)               VAR=settings.value(#VAR).toString()
#define PLOT_RESTORE_SETTING_STR2(VAR, DEFAULT)     VAR=settings.value(#VAR, DEFAULT).toString()
#define PLOT_RESTORE_SETTING_BOOL(VAR, DEFAULT)     VAR=settings.value(#VAR, DEFAULT).toBool()
#define PLOT_RESTORE_SETTING_INT(VAR, DEFAULT)      VAR=settings.value(#VAR, DEFAULT).toInt()

Set::Set(Db &db_): db(db_) { }

Set &Set::operator=(const Set &set_) {
	Q_ASSERT(&db==&set_.db);
	qDeleteAll(plots);
	plots.clear();
	for(const Plot *plot:set_.plots) { plots<<plot->clone(); }

	autoReplot=set_.autoReplot;
	plotMode=set_.plotMode;
	imageSize=set_.imageSize;
	stableColors=set_.stableColors;
	bbox=set_.bbox;
	globalVars=MapQStringQString(set_.globalVars);

	return *this;
}

Set::~Set() {
	qDeleteAll(plots);
	plots.clear();
}

Set *Set::clone() const {
	Set *ret=new Set(db);
	for(Plot *plot:plots) { ret->plots<<plot->clone(); }

	PLOT_CLONE_VAR(autoReplot);
	PLOT_CLONE_VAR(plotMode);
	PLOT_CLONE_VAR(imageSize);
	PLOT_CLONE_VAR(stableColors);
	PLOT_CLONE_VAR(bbox);
	ret->globalVars=MapQStringQString(globalVars);

	return ret;
}

void Set::setPlotMode(int plotMode_) {
	switch(Set::PlotMode(plotMode_)) {
	case QCustomPlot: case MyPlotter:
		plotMode=Set::PlotMode(plotMode_);
		break;
	default:
		qDebug()<<"on_cmbPlotMode_currentIndexChanged: WARNING: invalid index "<<plotMode_;
		plotMode=Set::PlotMode::QCustomPlot;
	}
}

void Set::store(QSettings &settings) const {
	settings.setValue("n-plots", plots.count());
	PLOT_STORE_SETTING(autoReplot);
	PLOT_STORE_SETTING(plotMode);
	PLOT_STORE_SETTING(imageSize);
	PLOT_STORE_SETTING(stableColors);
	PLOT_STORE_SETTING(bbox);
	{
		QByteArray data;
		QDataStream stream(&data, QIODevice::WriteOnly);
		stream<<globalVars;
		settings.setValue("variables", data);
	}
	for(auto plot:plots) { plot->store(settings); }
}

void Set::restore(QSettings &settings) {
	const int n_plots=settings.value("n-plots",1).toInt();
	PLOT_RESTORE_SETTING_BOOL(autoReplot, true);
	PLOT_RESTORE_SETTING_INT(plotMode, 0);
	setPlotMode(plotMode); // ensure that we always have a valid plot mode
	// We removed code to customize the image size 26/09/2018 10:46,
	// because I found out how to let the images resize themselves, and
	// to declutter the interface a bit.
	PLOT_RESTORE_SETTING_STR2(imageSize, "600,600");
	PLOT_RESTORE_SETTING_BOOL(stableColors, true);
	PLOT_RESTORE_SETTING_BOOL(bbox, false);
	{
		globalVars.clear();
		QByteArray data = settings.value("variables").toByteArray();
		QDataStream stream(&data, QIODevice::ReadOnly);
		stream >> globalVars;
	}

	qDeleteAll(plots);
	plots.clear();

	for(int i=0; i<n_plots; i++) {
		plots<<new Plot(*this);
		plots.back()->restore(settings);
	}
}


Plot::~Plot() {
	qDeleteAll(traces);
	traces.clear();
}

QString Plot::get_query(const MapQStringQString &override_vars) const { return apply_variables(defaultQuery, override_vars); }

QString Plot::getTableName() const { return tableFromQuery(get_query(MapQStringQString())); }
QString Plot::get_title(const MapQStringQString &override_vars) const { return apply_variables(title, override_vars); }
QString Plot::toString(const MapQStringQString &override_vars) const { return title.length() ? get_title(override_vars) : "Plot"; }
QString Plot::get_xAxisLabel(const MapQStringQString &override_vars) const { return apply_variables(xAxisLabel, override_vars); }
QString Plot::get_yAxisLabel(const MapQStringQString &override_vars) const { return apply_variables(yAxisLabel, override_vars); }
QString Plot::get_zAxisLabel(const MapQStringQString &override_vars) const { return apply_variables(zAxisLabel, override_vars); }

// idx might differ if some traces are disabled/invisible. This function converts
// an index with all visible traces, to an index where some traces are invisible.
// Useful e.g. in a legend table.
int Plot::allIdxToEnabledIdx(int allIdx) const {
    int result=0;
    for(int i=0; i<traces.size(); i++) {
        if (traces[i]->enabled) {
            if (result==allIdx) { return i; }
            result++;
        }
    }
    return -1;
}

int Plot::traceIdx(const Trace *trace, bool all) const {
	if (all) { return int(traces.indexOf(const_cast<Trace *>(trace))); }
	int ret=-1;
	for(auto *t:traces) {
		if (t->enabled) {
			ret++;
			if (t==trace) { return ret; }
		}
	}
	return -1;
}

QString Plot::plot_type() const {
	QString ret="";
	int trace_idx=-1;
	for(auto *trace:traces) {
		trace_idx++;
		if (trace->style) {
			if (ret=="") {
				ret=trace->style->plot_type();
			} else if (ret!=trace->style->plot_type()) {
				qDebug()<<"Currently using plot_type "<<ret<<", but trace "<<trace_idx<<" uses plot_type "<<trace->style->plot_type()
				       <<". Skipping.";
				return ret;
			}
		}
	}
	return ret;
}


Plot *Plot::clone() const {
	Plot *ret=new Plot(*set);

	PLOT_CLONE_VAR(defaultQuery);
	PLOT_CLONE_VAR(title);
	PLOT_CLONE_VAR(xAxisLabel); PLOT_CLONE_VAR(yAxisLabel); PLOT_CLONE_VAR(zAxisLabel);
	PLOT_CLONE_VAR(xmin); PLOT_CLONE_VAR(xmax); PLOT_CLONE_VAR(logX);
	PLOT_CLONE_VAR(ymin); PLOT_CLONE_VAR(ymax); PLOT_CLONE_VAR(logY);
	PLOT_CLONE_VAR(zmin); PLOT_CLONE_VAR(zmax); PLOT_CLONE_VAR(logZ);
	PLOT_CLONE_VAR(altColumns);
	PLOT_CLONE_VAR(altRows);
	ret->variables=MapQStringQString(variables);
	PLOT_CLONE_VAR(expanded);

	PLOT_CLONE_VAR(d3Grid);
	PLOT_CLONE_VAR(d3Mesh);
	PLOT_CLONE_VAR(d3Xrot);
	PLOT_CLONE_VAR(d3Zrot);

	for(Trace *trace:traces) {
		ret->traces<<trace->clone();
		ret->traces.back()->plot=ret;
	}
	return ret;
}

void Plot::store(QSettings &settings) const {
	settings.beginGroup(QString("plot-%1").arg(pos()[0]));
	PLOT_STORE_SETTING(title);
	PLOT_STORE_SETTING(defaultQuery);
	PLOT_STORE_SETTING(xAxisLabel); PLOT_STORE_SETTING(yAxisLabel); PLOT_STORE_SETTING(zAxisLabel);
	PLOT_STORE_SETTING(xmin); PLOT_STORE_SETTING(xmax); PLOT_STORE_SETTING(logX);
	PLOT_STORE_SETTING(ymin); PLOT_STORE_SETTING(ymax); PLOT_STORE_SETTING(logY);
	PLOT_STORE_SETTING(zmin); PLOT_STORE_SETTING(zmax); PLOT_STORE_SETTING(logZ);
	PLOT_STORE_SETTING(altColumns);
	PLOT_STORE_SETTING(altRows);
	{
		QByteArray data;
		QDataStream stream(&data, QIODevice::WriteOnly);
		stream<<variables;
		settings.setValue("variables", data);
	}
	PLOT_STORE_SETTING(expanded);
	PLOT_STORE_SETTING(d3Grid);
	PLOT_STORE_SETTING(d3Mesh);
	PLOT_STORE_SETTING(d3Xrot);
	PLOT_STORE_SETTING(d3Zrot);

	settings.setValue("n-traces", traces.count());
	for(auto trace:traces) { trace->store(settings); }
	settings.endGroup();
}

void Plot::restore(QSettings &settings) {
	settings.beginGroup(QString("plot-%1").arg(pos()[0]));
	PLOT_RESTORE_SETTING_STR(title);
	PLOT_RESTORE_SETTING_STR(defaultQuery);
	PLOT_RESTORE_SETTING_STR(xAxisLabel); PLOT_RESTORE_SETTING_STR(yAxisLabel); PLOT_RESTORE_SETTING_STR(zAxisLabel);
	PLOT_RESTORE_SETTING_STR(xmin); PLOT_RESTORE_SETTING_STR(xmax); PLOT_RESTORE_SETTING_BOOL(logX, false);
	PLOT_RESTORE_SETTING_STR(ymin); PLOT_RESTORE_SETTING_STR(ymax); PLOT_RESTORE_SETTING_BOOL(logY, false);
	PLOT_RESTORE_SETTING_STR(zmin); PLOT_RESTORE_SETTING_STR(zmax); PLOT_RESTORE_SETTING_BOOL(logZ, false);
	PLOT_RESTORE_SETTING_INT(altColumns,0);
	PLOT_RESTORE_SETTING_INT(altRows,0);
	{
		variables.clear();
		QByteArray data = settings.value("variables").toByteArray();
		QDataStream stream(&data, QIODevice::ReadOnly);
		stream >> variables;
	}
	PLOT_RESTORE_SETTING_BOOL(expanded, true);
	PLOT_RESTORE_SETTING_BOOL(d3Grid, false);
	PLOT_RESTORE_SETTING_INT(d3Mesh, 10);
	PLOT_RESTORE_SETTING_INT(d3Xrot, 30);
	PLOT_RESTORE_SETTING_INT(d3Zrot, 60);

	const int n_traces=settings.value("n-traces",1).toInt();
	for(int i=0; i<n_traces; i++) {
		traces<<new Trace(*this);
		traces.back()->restore(settings);
	}
	settings.endGroup();
}


void Plot::apply(MainWindow &mw) const {
	mw.ui->txtPlotDefaultQuery->setPlainText(defaultQuery);
	mw.ui->txtPlotTitle->setText(title);
	mw.ui->chkLogX->setChecked(logX);
	mw.ui->chkLogY->setChecked(logY);
	mw.ui->chkLogZ->setChecked(logZ);
	mw.ui->txtXAxisLabel->setText(xAxisLabel);
	mw.ui->txtYAxisLabel->setText(yAxisLabel);
	mw.ui->txtZAxisLabel->setText(zAxisLabel);
	mw.ui->txtXMin->setText(xmin); mw.ui->txtXMax->setText(xmax);
	mw.ui->txtYMin->setText(ymin); mw.ui->txtYMax->setText(ymax);
	mw.ui->txtZMin->setText(zmin); mw.ui->txtZMax->setText(zmax);
	mw.ui->txtAltColumns->setValue(altColumns);
	mw.ui->txtAltRows->setValue(altRows);

	mw.ui->chkD3Grid->setChecked(d3Grid);
	mw.ui->txtD3GridMesh->setValue(d3Mesh);
	mw.ui->txtXrot->setValue(d3Xrot);
	mw.ui->txtZrot->setValue(d3Zrot);
}

QString Plot::apply_variables(const QString &str, const MapQStringQString &override_vars) const {
	const MapQStringQString vars=merge(this->set->globalVars, merge(variables, override_vars));

	QString ret(str);
	for(int i=0; i<10 && ret.contains("${"); i++) {
		for(auto &k:vars.keys()) {
			ret=ret.replace(QString("${%1}").arg(k), vars[k]);
		}
	}

	return ret;
}

QList<int> Plot::pos() const { return QList<int>()<<set->plots.indexOf(const_cast<Plot *>(this)); }


Trace::Trace(Plot &plot_): plot(&plot_) {
}

Trace::~Trace() {
	delete style;
}

Trace *Trace::clone() const {
	Trace *ret=new Trace(*plot);
	ret->query=query;
	ret->x=x; ret->y=y; ret->z=z;
	ret->xtic=xtic; ret->ytic=ytic; ret->ztic=ztic;
	ret->style=style ? style->clone() : nullptr;
	ret->title=title;
	ret->enabled=enabled;
	ret->variables=variables;
	return ret;
}

void Trace::store(QSettings &settings) const {
	settings.beginGroup(QString("trace-%1-%2").arg(pos()[0]).arg(pos()[1]));
	settings.setValue("query", query);

	settings.setValue("x", x); settings.setValue("xtic", xtic);
	settings.setValue("y", y); settings.setValue("ytic", ytic);
	settings.setValue("z", z); settings.setValue("ztic", ztic);

	//	settings.setValue("type", type);
    settings.setValue("style", style ? style->serialize() : QByteArray());
	settings.setValue("title", title);
	settings.setValue("enabled", enabled);
	{
		QByteArray data;
		QDataStream stream(&data, QIODevice::WriteOnly);
		stream<<variables;
		settings.setValue("variables", data);
	}
	settings.endGroup();
}

void Trace::restore(QSettings &settings) {
	settings.beginGroup(QString("trace-%1-%2").arg(pos()[0]).arg(pos()[1]));
    query=settings.value("query", QString("SELECT * FROM %1").arg(DEFAULT_TABLE_NAME)).toString();

	x=settings.value("x").toString(); xtic=settings.value("xtic").toBool();
	y=settings.value("y").toString(); ytic=settings.value("ytic").toBool();
	z=settings.value("z").toString(); ztic=settings.value("ztic").toBool();

	//	type=settings.value("type", "histograms").toString();
	{
		auto data=settings.value("style").toByteArray();
		style=TraceStyle::deserialize(data);
	}
	title=settings.value("title", "Line title").toString();
	enabled=settings.value("enabled", true).toBool();
	{
		variables.clear();
		QByteArray data = settings.value("variables").toByteArray();
		QDataStream stream(&data, QIODevice::ReadOnly);
		stream >> variables;
	}
	settings.endGroup();
}

QList<int> Trace::pos() const { return QList<int>()<<plot->pos()[0]<<plot->traces.indexOf(const_cast<Trace *>(this)); }

QString Trace::apply_variables(const QString &str, const MapQStringQString &override_vars) const { return plot->apply_variables(str, merge(variables, override_vars)); }

QString remove_comments(const QString &str) {
	// Here we remove the lines that start with a '#', i.e. these are comments
	// If it occurs anywhere else but at the start, we don't consider it a comment
	//	ret=ret.split("\n").filter(QRegExp("^\\s*[^#]")).join(" ");

	// Here, we consider anything after a # a comment
	//	return ret.replace(QRegularExpression("#[^#\"']*$"),"").split("\n").join(" ").trimmed();
	auto rgx=QRegularExpression("#[^\n]*");
	return QString(str).replace(rgx,"").split("\n").join(" ").trimmed();
}

QString Trace::get_query(const MapQStringQString &override_vars) const {
	QString ret;
	if (remove_comments(query).length()==0) {
		ret=plot->get_query(merge(variables, override_vars));
	} else if (query.contains("-->")) {
		// In this case, we do a regex replacement in plot->defaultQuery
		ret=plot->defaultQuery;
		for(const QString &line:query.split("\n")) {
			const QStringList pts=line.split("-->");
			if (pts.size()<=1) { continue; }
			QRegularExpression search(pts[0].trimmed());
			QString repl=pts[1].trimmed();
			ret=ret.replace(search, repl);
		}
		ret=apply_variables(ret, override_vars);
	} else {
		ret=apply_variables(query, override_vars);
	}

    return remove_comments(ret);
}

QString Trace::getTableName() const { return tableFromQuery(get_query(MapQStringQString())); }

QString Trace::get_title(const MapQStringQString &override_vars) const {
	return title.length()
			? apply_variables(title, override_vars).replace("${_YLAB_}", y)
			: y; }

QString Trace::toString(const MapQStringQString &override_vars) const {
	return z.length()
			? QString("%1 (X:%2, Y:%3, Z:%4 [%5])").arg(get_title(override_vars)).arg(x).arg(y).arg(z).arg(style ? style->name() : "")
			: QString("%1 (X:%2, Y:%3 [%4])").arg(get_title(override_vars)).arg(x).arg(y).arg(style ? style->name() : "");
}

int Trace::dim() const { return (x.length()?1:0)+(y.length()?1:0)+(z.length()?1:0); }

QString ungroupQuery(QString query) {
	const QString aggregators="(COUNT|AVG|MIN|MAX|STDEV|SUM|LOWER_QUARTILE|MEDIAN|UPPER_QUARTILE)";
	query=query.replace(QRegularExpression("GROUP BY [a-zA-Z,_0-9- ]*", QRegularExpression::CaseInsensitiveOption), "");
	query=query.replace(QRegularExpression(aggregators+"\\([*]\\),",  QRegularExpression::CaseInsensitiveOption), "");
	query=query.replace(QRegularExpression(aggregators+"\\(",  QRegularExpression::CaseInsensitiveOption), "(");
	query=query.replace(QRegularExpression("PERCENTILE\\(([^,]*),[^)]*\\)",  QRegularExpression::CaseInsensitiveOption), "\\1");
	query=query.replace(QRegularExpression(",\\s*COUNT\\([^)]*\\)\\s* FROM",  QRegularExpression::CaseInsensitiveOption), " FROM");
	return query;
}

bool isSpecialQuery(const QString &q) {
	return q.startsWith("SPECIAL:",Qt::CaseInsensitive);
}

QStringList Trace::column_names(UseTraceQuery use_trace_query, enum UngroupQuery ungroup, const MapQStringQString &override_vars) const {
	QString query=use_trace_query==DoUseTraceQuery ? get_query(override_vars) : plot->get_query(merge(variables, override_vars));
	if (ungroup == UngroupQuery::Ungroup) { query=ungroupQuery(query); }
	if (isSpecialQuery(query)) { return QStringList(); }

	Db::Stmt q=plot->set->db.query(query);
	return q.column_names();
}

int Trace::column_index(const QString &col, const MapQStringQString &override_vars) const {
	return column_names(UseTraceQuery::DoUseTraceQuery, NoUngroup, override_vars).indexOf(col);
}

QList<QPair<QString,Trace::Rows>> Trace::mruCache;


void Trace::printCache(const QString &title) const {
	qDebug()<<"CACHE "<<title;
	for(const QPair<QString,Trace::Rows> &x:mruCache) {
		qDebug()<<"\t"<<x.first<<" rows:"<<x.second.size();
	}
	qDebug()<<"";
}

const Trace::Rows *Trace::findInCache(const QString &id) const {
	int idx=0;
	//    printCache("Before findInCache "+id);
	for(const QPair<QString,Trace::Rows> &x:mruCache) {
		if (x.first==id) {
			mruCache.move(idx, 0);
			return &mruCache[0].second;
		}
		++idx;
	}
	return nullptr;
}

const Trace::Rows *Trace::addToCache(const QString &id, const Trace::Rows &rows) const {
	Q_ASSERT(!findInCache(id));
	//    printCache("Before addToCache "+id+" "+QString::number(rows.size()));
	mruCache.push_front(QPair<QString,Rows>(id,rows));
	while (mruCache.size()>20) { mruCache.pop_back(); }
	//    printCache("After addToCache "+id+" "+QString::number(rows.size()));
	return &mruCache.front().second;
}

const QRegularExpression colVar("\\$\\[([a-zA-Z0-9_]+)\\]");
QString replaceSqlVars(const QString &str, const Trace::Row &row, const QStringList &colNames) {
	Q_ASSERT(row.size()==colNames.size());
	QRegularExpressionMatch m;
	QString ret=str;
	while ((m=colVar.match(ret)).hasMatch()) {
		const int i=int(colNames.indexOf(m.captured(1)));
		if (i>=0) {
			ret.replace(m.capturedStart(), m.capturedLength(), row[i].toString());
		} else {
			ret.replace(m.capturedStart(), m.capturedLength(), QString("[%1]").arg(m.captured(1)));
		}
	}
	return ret;
}
bool isString(const QVariant &variant) { return variant.userType() == QMetaType::QString; }

// If select_columns.length()==0, then we select all columns, else we limit it to the set ones.
const Trace::Rows *Trace::data(const QStringList &select_columns, UseTraceQuery use_trace_query, enum UngroupQuery ungroup, const MapQStringQString &override_vars) const {
	QString lquery=use_trace_query==DoUseTraceQuery ? get_query(override_vars) : plot->get_query(merge(variables, override_vars));
	//	qDebug()<<"data1 "<<lquery;
	//	qDebug()<<override_vars<<variables;
	if (ungroup==UngroupQuery::Ungroup) {
		//		DEBUG("Ungrouping query, from");
		//		DEBUG(query);
		lquery=ungroupQuery(lquery);
		//		DEBUG(" to ");
		MainWindow::instances[0]->log(lquery); // don't show quotes or escapes
	}

	if (isSpecialQuery(lquery)) { return nullptr; }

	// Something goes wrong in qt6 when caching, so we disabled it.
    const QString cacheId=QString("0x%1;%2;%3;%4;%5;%6")
		.arg(reinterpret_cast<uint64_t>(&plot->set->db))
        .arg(x).arg(y).arg(z).arg(select_columns.join(",")).arg(qHash(lquery));
	const Rows *cachedRows=findInCache(cacheId);
	if (cachedRows) {
		//qDebug()<<"Using cached version of "<<cacheId<<": "<<cachedRows->size()<<" rows";
		return cachedRows;
	}

	Db::Stmt q=plot->set->db.query(lquery);

	const QStringList col_names=q.column_names();

	QList<int> col_idxs;
	for(const QString &col:select_columns) {
		col_idxs<<int(col_names.indexOf(QRegularExpression(col, QRegularExpression::CaseInsensitiveOption)));
	}

	QList<QList<QVariant>> ret;
	if (select_columns.length()>0 && col_idxs.size()<2) { return nullptr; } // ensure x *and* y are set

	QList<QVariant> row;
	while ((row=q.next_row()).count()>0) {
		if (select_columns.length()>0) {
			QList<QVariant> cols;
			for(int col:col_idxs) { if (col>=0) { cols<<row[col]; } }

			if (select_columns.size()==cols.size()) {
				// This replaces a variable like $[VAR] with the contents of column VAR
				for(QVariant &col:cols) {
					if (isString(col) && col.toString().contains("$[")) {
						col=QVariant(replaceSqlVars(col.toString(), cols, select_columns));
					}
				}
			} else {
				qDebug()<<"Selected columns differs from the expected columns!";
				qDebug()<<"\tselected: "<<select_columns;
				qDebug()<<"\texpected: "<<cols;
			}
			ret<<cols;
		} else {
			ret<<row;
		}
	}

	//	qDebug()<<"Caching "<<cacheId<<" rows:"<<ret.size();
	//	qDebug()<<lquery;
	return addToCache(cacheId, ret);
}

unsigned Trace::rowCount(UseTraceQuery use_trace_query, enum UngroupQuery ungroup, const MapQStringQString &override_vars) const {
	QString query=use_trace_query==DoUseTraceQuery ? get_query(override_vars) : plot->get_query(merge(variables, override_vars));
	if (ungroup==UngroupQuery::Ungroup) { query=ungroupQuery(query); }
	if (isSpecialQuery(query)) { return 0; }
	Db::Stmt q=plot->set->db.query(query);
	return q.row_count();
}

void Trace::invalidateCache() {
	mruCache.clear();
}


MapQStringQString merge(const MapQStringQString &vars, const MapQStringQString &override) {
	MapQStringQString ret;
	for(auto x:vars.keys()) { ret[x]=vars[x]; }
	for(auto x:override.keys()) { ret[x]=override[x]; }
	return ret;
}
