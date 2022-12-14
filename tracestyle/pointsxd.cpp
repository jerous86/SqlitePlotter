#include "tracestyles.h"

#include <cmath>

#include <QPainter>

#include "plot.h"
#include "plot_myplotter.h"
#include "misc.h"

#include "helpers.h"
#include "plot_myqcustomplotter.h"

const MapQStringQString EmptyOverride=MapQStringQString();

struct PointsXD: public TraceStyle {
    void fmt_var(QString &label, const QString &var_name, const QString &var_value) const {
        label.replace(
                    QRegularExpression("(.*)"+QRegularExpression::escape(var_name)+"(.*)"),
                    "sprintf('%s%.3f%s', '\\1', "+var_value+", '\\2')"
                    );
    }

    QString fmt_label(const QString &label, const Trace &trace, int dim, const QString &xdelta, const QString &ydelta) const {
        (void) dim; (void) ydelta;
        QString label2=label;

        QStringList args;
        QRegularExpression rgxVar1=QRegularExpression("\\$(X|Y|Z|STDX|STDY)",QRegularExpression::CaseInsensitiveOption);
        while (label2.contains(rgxVar1)) {
            QRegularExpressionMatch m=rgxVar1.match(label2);
            Q_ASSERT(m.hasMatch());

            if (m.captured(1).toUpper()=="X") {
                args<<"$3"; label2.replace(m.capturedStart(0),m.capturedLength(0), "%.3f");
            } else if (m.captured(1).toUpper()=="Y") {
                args<<"$5"; label2.replace(m.capturedStart(0),m.capturedLength(0), "%.3f");
            } else if (m.captured(1).toUpper()=="Z") {
                args<<"$7"; label2.replace(m.capturedStart(0),m.capturedLength(0), "%.3f");
            } else if (m.captured(1).toUpper()=="STDX") {
                args<<"$8"; label2.replace(m.capturedStart(0),m.capturedLength(0), "%.3f");
            } else if (m.captured(1).toUpper()=="STDY") {
                args<<(xdelta.length()>0 ? "$9":"$8"); label2.replace(m.capturedStart(0),m.capturedLength(0), "%.3f");
            } else {
                DEBUG("Did not expect variable "<<m.captured(0));
                Q_ASSERT(false);
            }
        }
        label2=trace.apply_variables(label2,EmptyOverride);
        args.insert(0, "'"+label2+"'");
        return QString("(sprintf(%1))").arg(args.join(','));
    }


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

        QJSEngine &scriptEngine=state.myPlotter->scriptEngine;
        const double GNUPLOT_POINT_SIZE_MULT=5;
        const double point_size=style.get_opt_dbl(trace, "point-size", 2);
        const QString point_size_f=style.get_opt_str(trace, "point-size-f", "1").replace("log","Math.log").replace("pow","Math.pow").replace("sqrt","Math.sqrt");
        const bool connect=style.get_opt_bool(trace, "connect", true);
        const bool shade_below=style.get_opt_bool(trace, "shade-below", false);
        const bool connect_xy=style.get_opt_bool(trace, "connect-xy", false);
        const QString xdelta=style.get_opt_str(trace, "xdelta", "");
        const QString ydelta=style.get_opt_str(trace, "ydelta", "");


        const float W=DEFAULT_HISTOGRAM_BAR_WIDTH/state.series.size();
        const QVector3D xOffset=QVector3D(W*serieIdx-W,0,0);

        if (point_size>0) {
            for(const Trace::Row &row : serie.rows) {
                const QVector3D pt=getPt(trace, row)+xOffset;
                const float mult=float(scriptEngine.evaluate(QString(point_size_f).replace("$Z",QString::number(double(pt.z())))).toNumber());
                float finalPt=float(point_size*GNUPLOT_POINT_SIZE_MULT)*mult;
                if (!std::isfinite(finalPt) || finalPt<3) { finalPt=3; }
                scene.drawSymbol(p, plotIndex, pt, finalPt, color);
            }
        }
        if (connect) {
            p.setPen(QPen(color, isSelected ? 2 : 1));
            QVector3D prevPt=INVALID_VX;
            for(const Trace::Row &row : serie.rows) {
                const QVector3D pt=getPt(trace, row)+xOffset;
                if (prevPt!=INVALID_VX) {
                    QList<QVector3D> pts;
                    pts<<pt<<prevPt;
                    if (shade_below) { pts<<QVector3D(prevPt.x(),0,0)<<QVector3D(pt.x(),0,0); }
                    scene.drawFilledPolygon(p, objList, pts);
                }
                prevPt=pt;
            }
        }
        if (connect_xy) { // 3D only
            for(const Trace::Row &row : serie.rows) {
                const QVector3D pt=getPt(trace, row)+xOffset;
                scene.drawLine(p, objList, pt, QVector3D(pt.x(),pt.y(),0));
            }
        }

        if (xdelta.length()) {
            for(const Trace::Row &row : serie.rows) {
                const QVector3D pt=getPt(trace, row)+xOffset;
                const QVector3D err(row[serie.colIndex(xdelta)].toFloat(),0,0);
                scene.drawLine(p, objList, pt-err, pt+err);
            }
        }
        if (ydelta.length()) {
            for(const Trace::Row &row : serie.rows) {
                const QVector3D pt=getPt(trace, row)+xOffset;
                const QVector3D err(row[serie.colIndex(ydelta)].toFloat(),0,0);
                scene.drawLine(p, objList, pt-err, pt+err);
            }
        }
    }

    void handleMouseClick(MyPlotterState &state, int serieIdx, const QVector2D &mouse, QVector3D &closest, unsigned &bestDst) override {
        const Serie &serie=state.series[serieIdx];
        const Scene &scene=state.scene;

        const float W=DEFAULT_HISTOGRAM_BAR_WIDTH/state.series.size();
        const QVector3D xOffset=QVector3D(W*serieIdx-W,0,0);

        for(const Trace::Row &row : serie.rows) {
            const QVector3D pt=getPt(*serie.trace, row)+xOffset;
            const unsigned dst=unsigned(scene.doAll(pt).distanceToPoint(mouse));
            if (dst<bestDst) {
                closest=pt;
                bestDst=dst;
                state.selInfo=rowToString(state, row);
                state.selSeries=serieIdx;
            }
        }
    }
};

struct Points2D: public PointsXD {
    HELPER_2D(Points2D)
	QString help() const override { return
				"Draws 2D points."
				"\nOption: point-size [=2; double]"
				"\nOption: point-size-f [=''; string]: if non-empty, we multiply the evaluation of f. Available variables: $Z, $MAX_Z and $AVG_Z."
                "\nOption: connect [=true; bool]. If true, then draws a line (equivalent to Line)"
				"\nOption: label [=''; string]. If non-empty, then adds a label. Available variables: {$Z, $STDX, $STDY} (if applicable), and all defined ${VAR}s."
				"\nOption: label-first-point [=''; string]. If non-empty, then labels the first point. Available variables $Z, and all defined ${VAR}s."
				"\nOption: color [=''; string]. If non-empty, overrides the color of the points and line using 'rgb'."
				"\nOption: {xdelta,ydelta} [=''; string]. If non-empty, draws xerror resp. yerrorbars, using values from column xdelta resp. ydelta."
				"\nOption: {xoffset,yoffset} [=0; double]. Adds {xoffset,yoffset} to resp the {x,y} value."
				"\nOption: palette [=''; string]. If non-empty, uses column $palette to color points."
				"\nOption: draw-mean [=false; bool].If true, display the mean using a large symbol."
				"\nOption: draw-points [=true; bool]. If true, display the individual points."
				"\nOption: shade-below [=false; bool]. If true, and connect=true, then the area below the points is shaded.."
				;
				      }

	QStringList extra_columns(const Trace &trace) const override {
		QStringList ret;
		QStringList cols; cols<<"xdelta"<<"ydelta"<<"palette";
		for(const QString &x : cols) {
			const QString v=get_opt_str(trace, x,"");
			if (v.length()) { ret<<v; }
		}
		return ret;
	}

    QCPAbstractPlottable *plot_QCustomPlot(const Plot &plot, MyQCustomPlotter *p,
           const Trace &trace, int serieIdx,
           const QVector<double> &x, const QVector<double> &y, const QVector<double> &z,
           QVector<QCPAbstractPlottable *> &graphs) override {
        (void) graphs; (void) z;
        const TraceStyle &style=*trace.style;

        const int plotIndex=plot.set->stableColors ? plot.traceIdx(trace) : serieIdx;
        const double point_size=style.get_opt_dbl(trace, "point-size", 5);
        const bool connect=style.get_opt_bool(trace, "connect", true);

        QCPGraph *g=new QCPGraph(p->xAxis, p->yAxis);
        g->setLineStyle(connect ? QCPGraph::lsLine : QCPGraph::lsNone);
        g->setData(x, y);

        QCPScatterStyle myScatter;
        const int MAX_SHAPES = int(QCPScatterStyle::ScatterShape::ssPeace);
        myScatter.setShape(QCPScatterStyle::ScatterShape(2 + plotIndex % (MAX_SHAPES-2)));
        myScatter.setSize(point_size);
        g->setScatterStyle(myScatter);
        return g;
    }
};
void Points2D::no_vtable_warn() { }

struct Points3D: public PointsXD {
    HELPER_3D(Points3D)
    QString help() const override { return
                "Draws 3D points."
                "\nOption: point-size [=2; double]"
                "\nOption: connect [=false; bool]. If true, then draws a line (equivalent to Line)"
                "\nOption: connect-xy [=false; bool]. If true, then draws a line perpendicular to the XY-plane through each point"
                "\nOption: color [=''; string]. If non-empty, overrides the color of the points and line using 'rgb'."
                ;
    }

    QString plot_type() const override { return "splot"; }

    QCPAbstractPlottable *plot_QCustomPlot(const Plot &plot, MyQCustomPlotter *p,
           const Trace &trace, int serieIdx,
           const QVector<double> &x, const QVector<double> &y, const QVector<double> &z,
           QVector<QCPAbstractPlottable *> &graphs) override {
        (void) graphs; (void) z;
        const TraceStyle &style=*trace.style;

        const int plotIndex=plot.set->stableColors ? plot.traceIdx(trace) : serieIdx;
        const double point_size=style.get_opt_dbl(trace, "point-size", 5);
        const bool connect=style.get_opt_bool(trace, "connect", true);

        QCPGraph *g=new QCPGraph(p->xAxis, p->yAxis);
        g->setLineStyle(connect ? QCPGraph::lsLine : QCPGraph::lsNone);
        g->setData(x, y);

        QCPScatterStyle myScatter;
        const int MAX_SHAPES = int(QCPScatterStyle::ScatterShape::ssPeace);
        myScatter.setShape(QCPScatterStyle::ScatterShape(2 + plotIndex % (MAX_SHAPES-2)));
        myScatter.setSize(point_size);
        g->setScatterStyle(myScatter);
        return g;
    }
};
void Points3D::no_vtable_warn() { }

TraceStyle *newPoints2D() { return new struct Points2D(); }
TraceStyle *newPoints3D() { return new struct Points3D(); }
