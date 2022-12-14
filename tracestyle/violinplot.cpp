#include "tracestyles.h"

#include <cmath>

#include <QPainter>

#include "plot.h"
#include "plot_myplotter.h"
#include "misc.h"

#include "helpers.h"

struct Violin: public TraceStyle {
	HELPER_2D(Violin)
	QString help() const override { return "Draw violin plot. Requires that xtic is set.\n"
		"\nOption: n-bins [=10; int]. Number of bins in the frequency table."
		"\nOption: kernel [=0; enum {0,1}]. {0:gauss_kernel, 1:epanechnikov_kernel}"
		"\nOption: h [=0]. Bandwidth (Smoothing parameter). If h<=0 it is estimated"
		"\nOption: draw-mean-line [=1; bitset {0,1,2}]. {0: don't draw mean, 1: draw mean (blue), 2: draw median (red)}"
		"\nOption: draw-stats [=31; bitset {0,1,2,4,8,16}]. {0: don't draw anything, 1: P95/P5, 2: Q1/Q3, 4: mean, 8: median, 16: median[y>0]}"
		; }

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

		const size_t n_bins=size_t(style.get_opt_dbl(trace,"n-bins",10));
		const float violinplot_h=float(style.get_opt_dbl(trace, "h", 0));
		const int kernel_i=int(style.get_opt_dbl(trace, "kernel", 0));
		const NormalizeMode normalizeMode=NormalizeToMax;
		const int draw_mean_line=int(style.get_opt_dbl(trace, "draw-mean-line", 1));
		const int draw_stats=int(style.get_opt_dbl(trace, "draw-stats", 1+2+4+8+16));

		QMap<QString,QList<double>> data;
		double min=1e99, max=-1e99;
		for(const Trace::Row &row : serie.rows) {
			if (row.size()<=1) {
				qDebug()<<row;
				LOG_ERROR("Not enough columns to plot violinplot");
				return;
			}
			const double v=row[1].toDouble();
			if (std::isfinite(v)) {
				data[row[0].toString()].push_back(v);
				min=std::min(v, min);
				max=std::max(v, max);
			} else {
				LOG_WARNING("Non-finite value found "<<row[0].toString()<<" "<<v);
			}
		}

		if (min>=max) { return; }

		QMap<double,QList<QPointF>> freq_tables;
		for(const QVariant key:data.keys()) {
			// y is the frequency, x the bin
			const QList<QPointF> widths=freq_table(data[key.toString()], n_bins, min, max, normalizeMode);
			freq_tables[key.toDouble()]=widths;
		}


		QVector<double> x_offsets, medians, means;
		const float W=DEFAULT_HISTOGRAM_BAR_WIDTH/state.series.size();

		for(const QVariant key:data.keys()) {
			// pt is mostly useful to get the x-offset
			const float x_offset=getPt(trace, Trace::Row()<<key<</*just a number, but not 0 in case of log*/1).x()+serieIdx*W*2-W;
			x_offsets<<double(x_offset);
			p.setPen(QPen(color, isSelected ? 3 : 1));
			p.setBrush(QBrush(p.pen().color()));

			const ViolinPlotStats vpstats=violinplot_stats(data[key.toString()], n_bins, double(violinplot_h), kernel_i);
			const float step_size=vpstats.xs.size()>=2 ? vpstats.xs[1]-vpstats.xs[0] : 1;
			for(int i=0; i<vpstats.xs.size(); i++) {
				const QVector3D size(float(vpstats.ps[i])*W*2, step_size, 0);
				const QVector3D ptBotLeft(x_offset-size.x()/2, float(vpstats.xs[i]), 0);
				scene.drawFilledRect(p, objList, ptBotLeft, size);
			}

			means<<vpstats.mean;
			medians<<vpstats.median;

			// Draw the box plot stuff
			if (draw_stats & 0x01){ // (P5 -- P95)
				const float W=0.01;
				const auto pos=QVector3D(x_offset-W/2.0f, vpstats.p5,0);
				const auto size=QVector3D(W, vpstats.p95-vpstats.p5,0);
				p.setPen(Qt::black);
				p.setBrush(Qt::black);
				scene.drawFilledRect(p, objList, pos, size);
			}
			if (draw_stats & 0x02){ // (Q1 -- Q3)
				const float W=0.05;
				const auto pos=QVector3D(x_offset-W/2.0f, vpstats.q1,0);
				const auto size=QVector3D(W, vpstats.q3-vpstats.q1,0);
				p.setPen(Qt::gray);
				p.setBrush(Qt::gray);
				scene.drawFilledRect(p, objList, pos, size);
			}
			if (draw_stats & 0x04) { // mean
				const auto pos=QVector3D(x_offset, vpstats.mean,0);
				p.setPen(Qt::white);
				p.setBrush(Qt::white);
				scene.drawSphere(p, pos, 10);
			}
			if (draw_stats & 0x08) { // median
				const auto pos=QVector3D(x_offset, vpstats.median,0);
				p.setPen(Qt::blue);
				p.setBrush(Qt::blue);
				scene.drawSphere(p, pos, 7);
			}
			if (draw_stats & 0x10) { // median[y>0]
				const auto pos=QVector3D(x_offset, vpstats.mediangt0,0);
				p.setPen(Qt::green);
				p.setBrush(Qt::green);
				scene.drawSphere(p, pos, 2);
			}
		}

		if (draw_mean_line!=0) {
			Q_ASSERT(x_offsets.size()==means.size());
			Q_ASSERT(x_offsets.size()==medians.size());
			for(int i=0; i<x_offsets.size()-1; i++) {
				if (draw_mean_line & 0x01) {
					p.setPen(Qt::blue);
					QVector3D p1(x_offsets[i], means[i], 0), p2(x_offsets[i+1], means[i+1], 0);
					scene.drawLine(p, objList, p1, p2);
				}
				if (draw_mean_line & 0x02) {
					p.setPen(Qt::red);
					QVector3D p1(x_offsets[i], medians[i], 0), p2(x_offsets[i+1], medians[i+1], 0);
					scene.drawLine(p, objList, p1, p2);
				}
			}
		}
	}

	void handleMouseClick(MyPlotterState &, int , const QVector2D &, QVector3D &, unsigned &) override {
	}
};
void Violin::no_vtable_warn() { }


TraceStyle *newViolin() { return new struct Violin(); }
