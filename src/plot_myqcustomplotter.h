#pragma once

#include <QMouseEvent>

#include "qcustomplot.h"

extern const QPointF ptNull;
extern const QPointF pt0;
extern QPointF ch1; // crosshair
extern QPointF ch2; // rectangle "crosshair"

class MyQCustomPlotter: public QCustomPlot {
public:
    MyQCustomPlotter();

    static void repaintAll();
    QString label() const;

    QCPTextElement *title=nullptr;
	QVector<double> horizontals, verticals;

    void updateSelection(int i);
protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *e) override;
	
	  
	Q_SLOT void on_plottableClick (QCPAbstractPlottable *plottable, int, QMouseEvent *event);
	Q_SLOT void on_itemClick(QCPAbstractItem *item, QMouseEvent *event);
	Q_SLOT void on_legendClick(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event);
	Q_SLOT void on_selectionChangedByUser();
};
