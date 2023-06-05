#include "tracestyles.h"

#include <cmath>

#include <QPainter>
#include <QVector2D>

#include "plot.h"
#include "plot_myplotter.h"
#include "misc.h"

#include "helpers.h"

const float DEFAULT_VERTHISTOGRAM_WIDTH=1.f;

// The VertHistogram plot calculates and plots a histogram-like from data.
struct VertHistogram: public TraceStyle {
	HELPER_2D(VertHistogram)
	QString help() const override { return
				"Calculates histograms from the y-data, for each different x-value.\n"
				"\nOption: normalize-mode [=1; enum {0,1,2}]. {0: do not normalize, 1: normalize sum to 1, 2: normalize max to 1}"
				"\nOption: n-bins [=10; int]. Number of bins in the frequency table."
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
		NormalizeMode normalizeMode=NormalizeMode(int(style.get_opt_dbl(trace, "normalize-mode", 1)));
		if (int(normalizeMode)<0 || normalizeMode>2) {
			LOG_WARNING("Invalid normalizeMode "<<normalizeMode<<". Resetting to 1.");
			normalizeMode=NormalizeSumTo1;
		}

		QMap<QString,QList<double>> data;
		double min=1e99, max=-1e99;
		for(const Trace::Row &row : serie.rows) {
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

		float minPosFreq=1; // used for scaling, if we have logX
		QMap<QString,QList<QPointF>> freq_tables;
		for(const QVariant key:data.keys()) {
			// y is the frequency, x the bin
			const QList<QPointF> widths=freq_table(data[key.toString()], n_bins, min, max, normalizeMode);
			freq_tables[key.toString()]=widths;
			qDebug()<<min<<max<<"-->"<<key<<"-->"<<widths;
			for(auto &x:widths) { if (x.y()>0) { minPosFreq=std::min(minPosFreq, float(x.y())); } }
		}


		for(const QVariant key:data.keys()) {
			const QList<QPointF> &widths=freq_tables[key.toString()];
			const float bin_size=float(widths[1].x()-widths[0].x());
			// pt is mostly useful to get the x-offset
			const float x_offset=getPt(trace, Trace::Row()<<key<</*just a number, but not 0 in case of log*/1).x();
			p.setPen(QPen(color, isSelected ? 3 : 1));
			p.setBrush(QBrush(p.pen().color()));

			// widths[i].y contains the frequency of values in the bin widths[i].x.
			// So we want to draw y horizontally!
			for(int i=0; i<widths.size(); i++) {
				const auto freq=widths[i].y();
				const auto bin_pos=widths[i].x();
				const QVector3D size(
					DEFAULT_HISTOGRAM_BAR_WIDTH*(scene.logX ? mylog10(float(freq)/float(minPosFreq))/mylog10(1.f/float(minPosFreq)): float(freq)),
					bin_size,
					0);
				if (freq>0) {
					const QVector3D ptBotLeft(x_offset, float(bin_pos), 0);
					scene.drawFilledRect(p, objList, ptBotLeft, size);
				}
			}
		}
	}

	void handleMouseClick(MyPlotterState &state, int serieIdx, const QVector2D &mouse, QVector3D &closest, unsigned &bestDst) override {
		const Serie &serie=state.series[serieIdx];

		for(const Trace::Row &row : serie.rows) {
			const QString key=row[0].toString();
			const float x_offset=getPt(*serie.trace, Trace::Row()<<key<</*just a number, but not 0 in case of log*/1).x();
			const QVector3D pt=getPt(*serie.trace, row);
			if (mouse.x()>=x_offset && mouse.x()<=x_offset+DEFAULT_VERTHISTOGRAM_WIDTH) {
				const QVector3D ptCenter=pt;
				closest=ptCenter;
				state.selInfo=state.apply_variables(serie.trace->title, *serie.trace);
				state.selSeries=serieIdx;
				bestDst=0;
				break;
			}
		}
	}
};
void VertHistogram::no_vtable_warn() { }


TraceStyle *newVertHistogram() { return new struct VertHistogram(); }
