#include "plot_task.h"

#include <QDesktopServices>
#include <QProcess>
#include <QBuffer>
#include <QDir>
#include <QThread>
#include <QDebug>
#include <QString>
#include <QFont>

#include "misc.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

QMap<AspectRatioPixmapLabel *, MyPlotter *> GeneratePlotTask::myPlotters;

QStringList run(const QString &prog, const QStringList &args, int wait_ms) {
	QProcess *process=new QProcess();

	process->start(prog, args);
	if (wait_ms>=0) { process->waitForFinished(wait_ms); }

    const QString stderr2 = QString::fromLocal8Bit(process->readAllStandardError());

    if (stderr2.size() > 0) {
		LOG_ERROR("");
		LOG_ERROR("While executing "<<prog<<" "<<args.join(" "));
        LOG_ERROR(stderr2);
		LOG_ERROR("");
	}
	auto ret=QString::fromLocal8Bit(process->readAllStandardOutput()).split("\n");

	// Warning: memory leak. If we don't waitForFinished, then we don't delete process upon its exit.
	if (wait_ms>=0) { delete process; }

	return ret;
}

QString full_binary_path(const QString &binary) {
	QStringList paths=QString(getenv("PATH")).split(":");

	// In case PATH is not set correctly ...
	paths<<QString("/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin").split(":");

	for (const QString &path : paths) {
		if (QFile(path + "/" + binary).exists()) { return path + "/" + binary; }
	}
	return QString();
}

#include "plot_myplotter.h"
void GeneratePlotTask::run_myPlotter(const Plot &plot, const PlotOptions &opts, AspectRatioPixmapLabel *lblImage, const MapQStringQString &override_vars) {
	if (!myPlotters.contains(lblImage)) { myPlotters.insert(lblImage, new MyPlotter()); }
	MyPlotter *myPlotter=myPlotters[lblImage];
	myPlotter->init(plot, lblImage, override_vars);
	myPlotter->opts=opts;
	if (lblImage) {
		lblImage->plot_type=plot.plot_type();
		lblImage->d3Xrot=plot.d3Xrot;
		lblImage->d3Zrot=plot.d3Zrot;
	}

	for(const Trace *trace_:plot.traces) {
		const Trace &trace=*trace_;
		if (!trace.enabled) { continue; }

		QStringList select_columns=QStringList()<<trace.x<<trace.y;
		if (trace.z.length()) { select_columns<<trace.z; }
		if (trace.style) { select_columns.append(trace.style->extra_columns(trace)); }
        for(const QString &col : myPlotter->requiredColumns(trace)) {
			if (!select_columns.contains(col)) { select_columns<<col; }
		}

		const Trace::Rows *rows=trace.data(select_columns, DoUseTraceQuery, NoUngroup, override_vars);
		myPlotter->addData(trace, select_columns, *rows);
	}
	myPlotter->finish();
}

bool autoValue(const QString &x) { return x=="*" || x==""; }

QColor get_color(const Plot &plot, const Trace &trace) {
	const TraceStyle &style=*trace.style;
	
	const QString str_color=style.get_opt_str(trace, "color", "");
	const int plotIndex=plot.set->stableColors ? plot.traceIdx(trace) : plot.traceIdx(trace,false);
	return QColor(str_color.length() ? str_color : pick_color(plotIndex));
}

#include "plot_myqcustomplotter.h"
void GeneratePlotTask::run_QCustomPlot(const Plot &plot, const PlotOptions &opts, MyQCustomPlotter *p, const MapQStringQString &override_vars) {
    p->clearPlottables();
    p->clearItems();

    p->title->setVisible(opts.plotTitle);
    p->title->setText(plot.get_title(override_vars));

    double min_x=1e99, min_y=1e99, max_x=-1e99, max_y=-1e99;
    int serieIdx=-1;
    QVector<QCPAbstractPlottable *> graphs;

	p->horizontals.clear();
	p->verticals.clear();

    for(const Trace *trace_:plot.traces) {
        serieIdx++;
        const Trace &trace=*trace_;
        if (!trace.enabled) { continue; }
        if (trace.style==nullptr) { continue; }

        QStringList select_columns=QStringList()<<trace.x<<trace.y;
        if (trace.z.length()) { select_columns<<trace.z; }
        if (trace.style) { select_columns.append(trace.style->extra_columns(trace)); }

        QVector<double> x,y,z;
		const QString q=trace.get_query(override_vars);
		if (isSpecialQuery(q)) {
			{ QRegularExpressionMatch m=matchSpecialVertical.match(q);
				if (m.hasMatch()) {
					const float x=m.captured(1).toFloat();
					p->verticals<<x;
					// TODO don't ignore color
				}
			}

			{ QRegularExpressionMatch m=matchSpecialHorizontal.match(q);
				if (m.hasMatch()) {
					const float y=m.captured(1).toFloat();
					p->horizontals<<y;
					// TODO don't ignore color
				}
			}
		} else {
			const Trace::Rows *rows=trace.data(select_columns, DoUseTraceQuery, NoUngroup, override_vars);
			if (rows) {
				for(auto row:*rows) {
					if (row.size()<2) { continue; }
					x<<row[0].toDouble();
					y<<row[1].toDouble();
					if (row.size()>=3) { z<<row[2].toDouble(); }

					min_x=std::min(x.back(), min_x);
					min_y=std::min(y.back(), min_y);
					max_x=std::max(x.back(), max_x);
					max_y=std::max(y.back(), max_y);
				}

				Q_ASSERT(trace.style!=nullptr);
				TraceStyle &style=*trace.style;

				QCPAbstractPlottable *graph = style.plot_QCustomPlot(plot, p, trace, serieIdx, x, y, z, graphs);
				if (graph) {
					graph->setPen(get_color(plot, trace));
					graph->setName(trace.get_title(override_vars));
					graph->rescaleAxes();

					graphs<<graph;
					Q_ASSERT(p->plottableCount()>0);
				}
			}
		}
    }
	p->updateSelection(-1);

    p->legend->setVisible(opts.plotLegend);
    if (opts.plotLegend) {
        QFont legendFont;  // start out with MainWindow's font..
        legendFont.setPointSize(9); // and make a bit smaller for legend
        p->legend->setFont(legendFont);
    }

    p->xAxis->setScaleType(plot.logX ? QCPAxis::ScaleType::stLogarithmic  : QCPAxis::ScaleType::stLinear );
    p->yAxis->setScaleType(plot.logY ? QCPAxis::ScaleType::stLogarithmic  : QCPAxis::ScaleType::stLinear );

    p->xAxis->setLabel(plot.xAxisLabel);
    p->yAxis->setLabel(plot.yAxisLabel);
    // set axes ranges, so we see all data TODO use settings from window
    if (autoValue(plot.xmin) && autoValue(plot.xmax) && autoValue(plot.ymin) && autoValue(plot.ymax)) {
        p->rescaleAxes();
    } else {
        // TODO if we use stacked plots, {min,max}_{x,y} might be incorrect
        p->xAxis->setRange(autoValue(plot.xmin) ? min_x : plot.xmin.toDouble(), autoValue(plot.xmax) ? max_x : plot.xmax.toDouble());
        p->yAxis->setRange(autoValue(plot.ymin) ? min_y : plot.ymin.toDouble(), autoValue(plot.ymax) ? max_y : plot.ymax.toDouble());
    }
    p->replot();
    p->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom |QCP::iMultiSelect|QCP::iSelectPlottables|QCP::iSelectAxes |QCP::iSelectLegend);
}
