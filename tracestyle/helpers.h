#pragma once

#include <QString>

#define HELPER(CLASS, SUPPORTED_DIMS) \
    void no_vtable_warn() override; \
	int dim() const override { return SUPPORTED_DIMS; } \
    QString name() const override { return #CLASS; } \
    ETraceStyle style() const override { return ETraceStyle::CLASS; } \
    TraceStyle *clone() const override { return new CLASS(*this); }

#define HELPER_2D(CLASS) HELPER(CLASS, 1<<2)
#define HELPER_2D_3D(CLASS) HELPER(CLASS, 1<<2 | 1<<3)
#define HELPER_3D(CLASS) HELPER(CLASS, 1<<3)

#define LINE_SPLITTER "\\\n\t"

QString _using(const Trace &trace, bool use_x, bool use_y, QString use_z="", double x_offset=0, double y_offset=0);
QString _using2(const Trace &trace, QString x="${COL}", QString y="${COL}", QString use_z="", QStringList extraCols=QStringList(), double x_offset=0, double y_offset=0);

#include "plot.h"
bool eq(float l, float r);
QString pick_color(int i);
struct MyPlotterState;
QString rowToString(const MyPlotterState &state, const Trace::Row &row);
