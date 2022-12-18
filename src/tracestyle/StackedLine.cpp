#include "tracestyles.h"

#include <cmath>
#include <iostream>

#include <QPainter>

#include "helpers.h"

#include "plot_myqcustomplotter.h"

extern const MapQStringQString EmptyOverride;

QColor get_color(const Plot &plot, const Trace &trace);

struct StackedLine: public TraceStyle {
    HELPER_2D(StackedLine)
    QString help() const override { return
                "Draws stacked, shaded lines."
                "\nOption: width [=1; double] width of a bar"
                "\nOption: offset [COLUMN] if set to a clumn name, then the plot is shifted by the corresponding value in the column."
                ;
    }

    QString plot_type() const override { return "splot"; }
    void plot(QPainter &, const MyPlotterState &, int , ObjectsList &) override { }

    void handleMouseClick(MyPlotterState &, int , const QVector2D &, QVector3D &, unsigned &) override { }

    QCPAbstractPlottable *plot_QCustomPlot(const Plot &plot, MyQCustomPlotter *p,
           const Trace &trace, int serieIdx,
           const QVector<double> &x, const QVector<double> &y, const QVector<double> &z,
           QVector<QCPAbstractPlottable *> &graphs) override {
        (void) plot; (void) trace; (void) serieIdx;
        (void) z;
        QCPBars *g=new QCPBars(p->xAxis, p->yAxis);

        const double width=trace.style->get_opt_dbl(trace, "width", 1.0);
        const QString offset_column=trace.style->get_opt_str(trace, "offset", "").trimmed();

        g->setData(x, y);
        g->setWidth(width);
		g->setBrush(get_color(plot, trace).lighter(110));

        if (offset_column.length()!=0) {
            const Trace::Rows *offset_data=trace.data(QStringList()<<trace.x<< offset_column,DoUseTraceQuery,NoUngroup,EmptyOverride);
            if (offset_data!=nullptr) {
                QVector<double> y2;
                for(const Trace::Row &r : *offset_data) {
                    Q_ASSERT(r.size()==2);
                    y2<<r[1].toDouble();
                }

                QCPBars *g=new QCPBars(p->xAxis, p->yAxis);
                g->setData(x, y2);
                g->setWidth(width);
                g->setName("Offset "+trace.get_title(EmptyOverride));

                if (graphs.size()>0) {
                    QCPBars *below=dynamic_cast<QCPBars *>(graphs.back());
                    if (below) { g->moveAbove(below); }
                }
                graphs<<g;
            }
        }

        if (graphs.size()>0) {
            QCPBars *below=dynamic_cast<QCPBars *>(graphs.back());
            if (below) {
                g->moveAbove(below);
            }
        }
        return g;
    }
};
void StackedLine::no_vtable_warn() { }

TraceStyle *newStackedLine() { return new struct StackedLine(); }
