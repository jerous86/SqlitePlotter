#pragma once

#include <QString>
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QTextEdit>
#include <QRegularExpression>

class MainWindow;
class AspectRatioPixmapLabel;
class MyQCustomPlotter;
struct MyPlotter;

#include "plot.h"

const QRegularExpression matchSpecialVertical  =QRegularExpression  ("SPECIAL:VERTICAL\\(([^,]*)(,([^,]*)(,(.*))?)?\\)");
const QRegularExpression matchSpecialHorizontal=QRegularExpression("SPECIAL:HORIZONTAL\\(([^,]*)(,([^,]*)(,(.*))?)?\\)");

void init_trace_styles();
void deinit_trace_styles();

// We keep these apart in case we want to apply globally (e.g. for inclusion in paper)
struct PlotOptions {
	bool plotTitle=true;
    bool plotLegend=true;
};

class MainWindow;
struct GeneratePlotTask {
	MainWindow *window;
	const int i;

    GeneratePlotTask(MainWindow *window_, int i_): window(window_), i(i_) { }

    static QMap<AspectRatioPixmapLabel *, MyPlotter *> myPlotters;
    void run_myPlotter(const Plot &plot, const PlotOptions &opts, AspectRatioPixmapLabel *lblImage, const MapQStringQString &override_vars);
    void run_QCustomPlot(const Plot &plot, const PlotOptions &opts, MyQCustomPlotter *lblImage, const MapQStringQString &override_vars);
};

