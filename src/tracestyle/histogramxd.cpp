#include "tracestyles.h"

#include <QPainter>

#include "plot.h"
#include "plot_myplotter.h"

#include "helpers.h"
#include "myplotter_helpers.h"

struct HistogramxD: public TraceStyle {
	QString help() const override { return
		"Draw histogram. Requires that xtic is set."
		"\nOption: full-bars [=true; bool] "
		;
	}
	QString plot_type() const override { return "plot"; }

    void plot(QPainter &p, const MyPlotterState &state, int serieIdx, ObjectsList &objList) override {
		const Plot *plot=state.plot;
		const Serie &serie=state.series[serieIdx];
		const Scene &scene=state.scene;
		const Trace &trace=*plot->traces[state.traceIdx[serieIdx]];
		const TraceStyle &style=*trace.style;

		const QString str_color=style.get_opt_str(trace, "color", "");
		const int plotIndex=plot->set->stableColors ? plot->traceIdx(trace) : serieIdx;
		const QColor color=QColor(str_color.length() ? str_color : pick_color(plotIndex));
		const bool isSelected=serieIdx==state.selSeries;

		SET_RANGE_VAR_MIN(ymin,1); SET_RANGE_VAR_MAX(ymax,1);
		if (eq(ymin,ymax)) { ymax=ymin+1; }

		const float W=DEFAULT_HISTOGRAM_BAR_WIDTH/state.series.size();
		const float W_Y=(ymax-ymin)/20;
		const bool full_bars=style.get_opt_bool(trace, "full-bars", true);
		const bool hasErrBars=trace.z.length()>0;

		for(const Trace::Row &row : serie.rows) {
			const QVector3D pt=getPt(trace, row);
			// NOTE: do not use plotIndex here. These entries are not color-stable.
			const float y1 = full_bars ? (-pt[1] + (plot->logY ? ymin : 0)) : 0;
			const float y2 = full_bars ? 0                                  : W_Y;
			const QVector3D lb = pt + QVector3D( W+serieIdx*W*2, y1, -pt[2]);
			const QVector3D ub = pt + QVector3D(-W+serieIdx*W*2, y2, 0);
			// We only draw if lb<ub, because using logarithms on zero values might give lb>ub!
			if (lb.y()<ub.y()) {
				p.setPen(QPen(Qt::black, isSelected ? 3 : 1));
				p.setBrush(QBrush(color));
				scene.drawBox(p, objList, ub, lb);
			}
			if (hasErrBars) {
				// Also draw error bars
				QVector3D center=ub;
				center.setX(center.x()+W);
				const float err=row[2].toFloat();
				scene.drawLine(p, objList, center-QVector3D(0,err,1),center+QVector3D(0,err,1));
			}
		}
	}

	void handleMouseClick(MyPlotterState &state, int serieIdx, const QVector2D &mouse, QVector3D &closest, unsigned &bestDst) override {
		const Plot *plot=state.plot;
		const Serie &serie=state.series[serieIdx];
		const Scene &scene=state.scene;

		for(const Trace::Row &row : serie.rows) {
			const float ymin=0.f;
            const float W=DEFAULT_HISTOGRAM_BAR_WIDTH/state.series.size();
			const QVector3D
                pt=getPt(*serie.trace, row),
				lb=pt+QVector3D(W+serieIdx*W*2,-pt[1]+(plot->logY ? ymin : 0), -pt[2]),
				ub=pt+QVector3D(-W+serieIdx*W*2, 0, 0);
			const QPoint
				lbp=scene.doAll(lb).toPoint(),
				ubp=scene.doAll(ub).toPoint();
			if (ubp.x()<=mouse.x() && mouse.x()<=lbp.x() && ubp.y()<=mouse.y() && mouse.y()<=lbp.y()) {
				const float yOffset=pt.y()*0.05f;
				const QVector3D ptCenter=pt+QVector3D(serieIdx*W*2, yOffset, 0);
				closest=ptCenter;
				state.selInfo=rowToString(state, row)+" "+state.apply_variables(serie.trace->title, *serie.trace);
				state.selSeries=serieIdx;
				bestDst=0;
				break;
			}
		}
	}
};

struct Histogram2D: public HistogramxD {
    HELPER_2D(Histogram2D)
};
void Histogram2D::no_vtable_warn() { }

struct Histogram3D: public HistogramxD {
    HELPER_3D(Histogram3D)
	QString help() const override { return
				"Draw histogram. Requires that xtic is set."
				"\nOption: full-bars: [true; bool]: draw bars from origin. If false, show squares with height."
				; }
	QString plot_type() const override { return "splot"; }
};
void Histogram3D::no_vtable_warn() { }

TraceStyle *newHistogram2D() { return new struct Histogram2D(); }
TraceStyle *newHistogram3D() { return new struct Histogram3D(); }
