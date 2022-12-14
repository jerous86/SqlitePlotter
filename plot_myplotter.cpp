#include "plot_myplotter.h"

#include <math.h>
#include <cmath>

#include <QVector3D>
#include <QPainter>
#include <QLabel>
#include <QPixmap>

#include "my_math.h"
#include "aspectratiopixmaplabel.h"
#include "mainwindow.h"
#include "misc.h"
#include "myplotter_helpers.h"

const int LEGEND_TEXT_HEIGHT=15;



const QRegularExpression colVar("\\$\\[([a-zA-Z0-9_]+)\\]");

QMap<AspectRatioPixmapLabel *, MyPlotterState> MyPlotter::states;

MyPlotter::MyPlotter() { }

MyPlotter::~MyPlotter() {  }

void MyPlotter::init(const Plot &plot, AspectRatioPixmapLabel *lblImage, const MapQStringQString &override_vars) {
	curImg=lblImage;
	curImg->setMouseTracking(true);

	if (!states.contains(curImg)) {
		curImg->installEventFilter(this);
		states.insert(curImg, MyPlotterState());
	}
	MyPlotterState &state=states[curImg];
	state.plot=&plot;
	state.myPlotter=this;
	state.override_vars=&override_vars;
	Q_ASSERT(state.plot);
	state.series.clear();
	state.traceIdx.clear();
	state.pxm=QPixmap(400,400); // it is updated in repaint().
	state.scene.logX=plot.logX;
	state.scene.logY=plot.logY;
	state.scene.logZ=plot.logZ;
	state.bbLB=state.bbUB=INVALID_VX;
	mouseOffset=tmpMouseOffset=QPoint();
	xTicks[&plot].clear(); yTicks[&plot].clear(); zTicks[&plot].clear();
	xTicks[&plot]<<""; // this shift the range conveniently a bit to the right, so the numbers on the y-axis are
		// readable by default.
//	state.scene.zoom=0.9;
	state.scene.d2Init();
	state.scene.d3Init();
}

void MyPlotter::addData(const Trace &trace, const QStringList &columns, const Trace::Rows &rows) {
	Q_ASSERT(states.contains(curImg));
	MyPlotterState &state=states[curImg];

	Serie serie;

	const QString q=trace.get_query(*state.override_vars);
	if (isSpecialQuery(q)) {
		{ QRegularExpressionMatch m=matchSpecialVertical.match(q);
			if (m.hasMatch()) {
				// TODO don't ignore color
				const float v=m.captured(1).toFloat();
				serie.vertical.enable(v);
			}
		}

		{ QRegularExpressionMatch m=matchSpecialHorizontal.match(q);
			if (m.hasMatch()) {
				// TODO don't ignore color
				const float v=m.captured(1).toFloat();
				serie.horizontal.enable(v);
			}
		}
	} else {
		serie.trace=&trace;
		serie.columnNames=columns;
		serie.rows=rows;

		for(auto &row:rows) {
			const auto pt=getPt(trace, row);
			state.bbLB=minPW(state.bbLB,pt);
			state.bbUB=maxPW(state.bbUB,pt);
		}
	}
	state.series<<serie;
	state.traceIdx<<int(trace.plot->traces.indexOf(const_cast<Trace *>(&trace)));
}

void MyPlotter::finish() {
	Q_ASSERT(states.contains(curImg));
	setBoundaries(states[curImg]);
	repaint(curImg);
	curImg=nullptr;
}

void MyPlotter::invalidateCache(AspectRatioPixmapLabel *l) {
	if (states.contains(l)) {
		MyPlotterState &state=states[l];
		state.gridWeights.clear();
	}
}

bool MyPlotter::isUsed(const AspectRatioPixmapLabel *l) { return states.contains(const_cast<AspectRatioPixmapLabel *>(l)); }

MyPlotter *MyPlotter::getPlotter(const AspectRatioPixmapLabel *l) {
	if (!states.contains(const_cast<AspectRatioPixmapLabel *>(l))) { return nullptr; }
	return states[const_cast<AspectRatioPixmapLabel *>(l)].myPlotter;
}

QList<QPair<float,QString>> niceAxisTicks(const QString &debug, const TickMap &tickMap, bool isLog, float start, float stop) {
	QList<QPair<float,QString>> ret;
	if (start>=stop) { LOG_WARNING("niceAxisTicks("<<debug<<"): !! start>=stop !! "<<isLog<<start<<stop); return ret; }
	if (!std::isfinite(start) || !std::isfinite(stop)) {
		LOG_WARNING("niceAxisTicks("<<debug<<"): !! start or stop are not finite !! "<<isLog<<start<<stop);
		return ret;
	}
	Q_ASSERT(start<stop);

	if (tickMap.size()>0) {
		for(int i=0; i<tickMap.size(); i++) {
			ret<<QPair<float,QString>(i,tickMap[i]);
		}
		return ret;
	}

	if (isLog) {
		start=floor(mylog10(start));
		stop=ceil(mylog10(stop));
		for(int i=int(start); i<stop; i++) {
			ret<<QPair<float,QString>(mypow10(i),QString("1e%1").arg(i));

			// Add nameless ticks
			for(int j=2; j<10; j++) {
				ret<<QPair<float,QString>(j*mypow10(i),"");
			}
		}
	} else {
		float scale_mult=1;
		const float w=stop-start;
		while (true) {
			const float scale=std::pow(10.f, floor(log(w)/log(10.f)))*scale_mult;
			const int N=int(ceil((stop-start)/scale));
			if (N<5) {
				scale_mult*=0.5f;
			} else {
				start=floor(start/scale)*scale;

				for(int i=0; i<=N; i++) {
					float v=start+i*scale;
					ret<<QPair<float,QString>(v,QString::number(double(v)));
				}
				break;
			}
		}
	}
	return ret;
}

void drawAxis3D(QPainter &p, const Scene &scene, ObjectsList &objList, const QVector3D &unit, const QString &label, const TickMap &tickMap, bool isLog, float start, float stop,
		const QVector3D &tickOffset, const QVector3D &labelOffset) {
	p.setPen(Qt::black);
	scene.drawLine(p, objList, QVector3D(), unit*stop);
	scene.drawText(p, unit*stop+labelOffset, label, 12);

	if (eq(start,stop)) { LOG_WARNING("axis "<<label<<" has equal start and stop ("<<start<<")"); return; }
	if (start>stop) { LOG_WARNING("axis start > stop: "<<start<<" > "<<stop); return; }

	for(QPair<float,QString> v : niceAxisTicks(toString(unit), tickMap, isLog, start, stop)) {
		scene.drawText(p, unit*v.first+tickOffset, v.second);
	}
}

template<class T=float>
T interpol(T x1,T y1, T x2, T y2, T x) { return (y2-y1)/(x2-x1)*(x-x1)+y1; }

const int TITLE_HEIGHT=20;
const int H=20;
const int LEFT_GUTTER=50;

void drawRotatedText(QPainter &p, const QPoint rotCenter, const int deg, const QString &txt) {
	p.save();
	p.translate(rotCenter);
	p.rotate(deg);
	p.drawText(QPoint(), txt);
	p.restore();
}

void drawRotatedText(QPainter &p, const QPoint rotCenter, QSize s, const int deg, const QString &txt, QTextOption option=QTextOption(Qt::AlignmentFlag::AlignCenter)) {
	p.save();
	p.translate(rotCenter);//-QPoint(s.width()/2,s.height()/2));
	p.rotate(deg);
	p.drawText(QRect(0,0,s.width(),s.height()), txt, option);
	p.restore();
}


void drawAxis2D(QPainter &p, const QPixmap &pxm, const Scene &scene, const QVector3D &unit, const QString &label, const TickMap &tickMap, bool isLog, float start, float stop) {
	p.setPen(Qt::black);
	const bool isX=!eq(unit[0], 0);

	if (eq(start,stop)) { LOG_WARNING("axis "<<label<<" has equal start and stop ("<<start<<")"); return; }
	if (start>stop) { LOG_WARNING("axis start > stop: "<<start<<" > "<<stop); return; }

	const float
		Plo=scene.doAll(unit*start)[isX ? 0 : 1],
		Phi=scene.doAll(unit*stop)[isX ? 0 : 1],
		lo=isX ? LEFT_GUTTER : pxm.height(),
		hi=isX ? pxm.width() : H,
		newStart=interpol(Plo, start, Phi, stop, lo),
		newStop=interpol(Plo, start, Phi, stop, hi);

	QFontMetrics fm(p.font());
	const auto ticks=niceAxisTicks(toString(unit), tickMap, isLog, isLog ? start : newStart, isLog ? stop : newStop);
//	const int xLabelRot=(-45);
	const int xLabelRot=-std::min(int(ticks.size()),10)*9;

	if (isX) {
		p.drawText(QPoint(pxm.width()/2,pxm.height()), label);
	} else {
		drawRotatedText(p, QPoint(H,pxm.height()/2), xLabelRot, label);
	}

	QPoint maxTextSize(0,0);
#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
	for(QPair<float,QString> v : ticks) {
		//		const QPoint s(fm.boundingRect(v.second).width(), fm.height());
		const QPoint s=QPoint(fm.horizontalAdvance(v.second), fm.height());
		maxTextSize=maxPW(maxTextSize, s);
	}
#else
	qDebug()<<"TODO drawAxis2D";
#endif

	QPoint tickOffset;
	if (isX) { tickOffset.setY(-int(maxTextSize.y()*sin(deg2rad(90)))); }

	for(QPair<float,QString> v : ticks) {
		QPoint pt=scene.doAll(unit*v.first).toPoint();
		const QString tick=v.second;
		if (isX) {
			pt.setY(pxm.height()-10);
			const QSize size(int(maxTextSize.x()*1.1), int(maxTextSize.x()*1.1));
			drawRotatedText(p, pt+tickOffset-QPoint(10,0), size,
					xLabelRot, tick, QTextOption(Qt::AlignmentFlag::AlignLeft));
		} else {
			pt.setX(H);
			p.drawText(QRect(pt.x(), pt.y()-H/2, LEFT_GUTTER, H), tick, QTextOption(Qt::AlignmentFlag::AlignRight));
			pt+=QPoint(LEFT_GUTTER+2,0);
		}

		// if tickMaps.size()>0 then we have ?ticlabels set.
		p.drawLine(pt, pt+(isX ? QPoint(0,1) : QPoint(1,0))*(tickMap.size() ? 20 : 10));
	}


	// Indicate the original window range
	p.setPen(Qt::red);
	if (isX) {
		const int x0=int(Plo), x1=int(Phi), y0=pxm.height()-H*2, y1=y0;
		p.drawLine(x0,y0,x1,y1);
	} else {
		const int y0=int(Plo), y1=int(Phi), x0=LEFT_GUTTER+H/2+H+2, x1=x0;
		p.drawLine(x0,y0,x1,y1);
	}
}

bool hasVertHist(const MyPlotterState &state) {
	const Plot *plot=state.plot;
	for(int serieIdx=0; serieIdx<state.series.size(); serieIdx++) {
		const int i=state.traceIdx[serieIdx];
		if (i>=plot->traces.size()) { continue; }
//		qDebug()<<serieIdx<<"/"<<state.series.size()<<" "<<i<<"/"<<plot->traces.size()<<" "<<plot<<" "<<&plot->traces;
//		qDebug()<<"    "<<plot->traces[i];
		const Trace &trace=*plot->traces[i];
		if (!trace.style) { continue; }

		if (trace.style->style()==ETraceStyle::VertHistogram || trace.style->style()==ETraceStyle::Violin) { return true; }
	}
	return false;
}


void MyPlotter::repaint(AspectRatioPixmapLabel *lblImage) {
	Q_ASSERT(states.contains(lblImage));
	MyPlotterState &state=states[lblImage];

	QSize size=lblImage->sizeHint().width()>0 ? lblImage->sizeHint() : lblImage->size();
	if (lblImage->keepAspectRatio()) {
		const auto s=std::min(size.width(), size.height());
		size=QSize(s,s);
	}
	state.pxm=QPixmap(size);
	state.pxm.fill(Qt::white);
	state.scene.is3D=lblImage->plot_type=="splot";

//	qDebug()<<lblImage<<&state<<state.plot;
	repaint3D(lblImage);
}

QStringList MyPlotter::requiredColumns(const Trace &trace) const {
	auto *style=trace.style;

	QStringList ret;
	const QString xdelta=style ? style->get_opt_str(trace, "xdelta", "") : "";
	if (xdelta.length()) { ret<<xdelta; }

	const QString ydelta=style ? style->get_opt_str(trace, "ydelta", "") : "";
	if (ydelta.length()) { ret<<ydelta; }

	auto it=colVar.globalMatch(trace.get_query(EmptyMap));
	while (it.hasNext()) {
		auto m=it.next();
		const QString var=m.captured(1);
		if (!ret.contains(var)) { ret<<var; }
	}

	return ret;
}

void MyPlotter::setBoundaries(MyPlotterState &state) {
	const Plot *plot=state.plot;
	Scene &scene=state.scene;
	const bool is3D=scene.is3D;

	SET_RANGE_VAR_MIN(xmin,0); SET_RANGE_VAR_MAX(xmax,0);
	SET_RANGE_VAR_MIN(ymin,1); SET_RANGE_VAR_MAX(ymax,1);
	SET_RANGE_VAR_MIN(zmin,2); SET_RANGE_VAR_MAX(zmax,2);

	if (eq(xmin,xmax) || xmax<xmin) { xmax=xmin+1; }
	if (eq(ymin,ymax) || ymax<ymin) { ymax=ymin+1; }
	if (eq(zmin,zmax) || zmax<zmin) { zmax=zmin+1; }
	if (!is3D) { zmin=0; zmax=1; }

	// See NOTE: vertHist-LogX
	ValueRestorer<bool> logX(scene.logX); // WARNING: changing const-ness here!
	if (hasVertHist(state)) { logX.set(false); }

	const QVector3D ulimit(xmax,ymax,zmax), llimit(xmin,ymin,zmin);
	const QList<QVector3D> units=QList<QVector3D>()<<QVector3D(1,0,0)<<QVector3D(0,1,0)<<QVector3D(0,0,1);
	const QList<QVector3D> axes=QList<QVector3D>()<<units[0]*xmax<<units[1]*ymax<<units[2]*zmax;

	{ // initialize
		// Do like in gnuplot, where the xy-plane is square
		const float size=std::max(xmax-xmin, ymax-ymin);

		scene.d3Init();

		const bool xMinSet=!(plot->xmin=="*"||plot->xmin=="");
		const bool xMaxSet=!(plot->xmax=="*"||plot->xmax=="");
		const bool yMinSet=!(plot->ymin=="*"||plot->ymin=="");
		const bool yMaxSet=!(plot->ymax=="*"||plot->ymax=="");

		// use*[0:min-x, 1:max-x, 2:min-y, 3:max-y]
		const QVector<bool> useAxes=QVector<bool>()<<!xMinSet<<!xMaxSet<<!yMinSet<<!yMaxSet;
		const QVector<bool> useSeries=useAxes;
		const QVector<bool> useLimits=QVector<bool>()<<xMinSet<<xMaxSet<<yMinSet<<yMaxSet;

		const float scaleMult=15;

		const QPoint finalMouse=mouseOffset+tmpMouseOffset;
		if (is3D) {
			const float scalex=scaleMult*size/(xmax-xmin);
			const float scaley=scaleMult*size/(ymax-ymin);
			const float scalez=scaleMult*size/(zmax-zmin);

			scene.d3SetVp(QVector3D(axes[0].x()/2, axes[1].y()/2, 0));
			scene.d3Rot=rotX(deg2rad(float(plot->d3Xrot+finalMouse.y()))) * rotZ(float(deg2rad(plot->d3Zrot+finalMouse.x())));
			scene.d3Scale=scale3D(scalex,scaley,scalez);
			scene.set2DTransformation(state.pxm, axes, state.series, llimit, ulimit, useAxes, useSeries, useLimits);
		} else { // Always look at the XY-plane.
			const float scalex=scaleMult*size/(xmax-xmin);
			const float scaley=scaleMult*size/(ymax-ymin);
			const float scalez=1;

			scene.d3Rot=rotX(deg2rad(0)) * rotZ(deg2rad(0)) * rotY(float(deg2rad(0)));
			scene.d3Scale=scale3D(scalex,scaley,scalez);
			scene.set2DTransformation(state.pxm, axes, state.series, llimit, ulimit, useAxes, useSeries, useLimits);
			scene.d2Translate(finalMouse.x(), finalMouse.y());
		}
	}
}

void drawAxis(QPainter &p, MyPlotterState &state, ObjectsList &objList, float xmin, float xmax, float ymin, float ymax, float zmin, float zmax) {
	const Plot *plot=state.plot;
	Scene &scene=state.scene;
	const bool is3D=scene.is3D;

	const QList<QVector3D> units=QList<QVector3D>()<<QVector3D(1,0,0)<<QVector3D(0,1,0)<<QVector3D(0,0,1);
	const QList<QVector3D> axes=QList<QVector3D>()<<units[0]*xmax<<units[1]*ymax<<units[2]*zmax;

	p.setPen(Qt::black);
	const QString xaxislabel=QString("%1 (X)").arg(plot->xAxisLabel.length()>0 || plot->traces.size()==0
		? state.apply_variables(plot->xAxisLabel)
		: state.apply_variables(plot->traces[0]->x));
	const QString yaxislabel=QString("%1 (Y)").arg(plot->yAxisLabel.length()>0 || plot->traces.size()==0
		? state.apply_variables(plot->yAxisLabel)
		: state.apply_variables(plot->traces[0]->y));
	if (is3D) {
		const QString zaxislabel=QString("%1 (Z)").arg(plot->zAxisLabel.length()>0 || plot->traces.size()==0
			? state.apply_variables(plot->zAxisLabel)
			: state.apply_variables(plot->traces[0]->z));
		drawAxis3D(p, scene, objList, units[0], xaxislabel, xTicks[plot], scene.logX, xmin, xmax, QVector3D(0,0,-(zmax-zmin)/20), QVector3D());
		drawAxis3D(p, scene, objList, units[1], yaxislabel, yTicks[plot], scene.logY, ymin, ymax, QVector3D(0,0,-(zmax-zmin)/20), QVector3D());
		drawAxis3D(p, scene, objList, units[2], zaxislabel, zTicks[plot], scene.logZ, zmin, zmax, QVector3D(-(xmax-xmin)/20,0,0), QVector3D());
		scene.drawLine(p, objList, axes[0], axes[0]+axes[1]);
		scene.drawLine(p, objList, axes[1], axes[0]+axes[1]);
	} else {
		drawAxis2D(p, state.pxm, scene, units[0], xaxislabel, xTicks[plot], scene.logX, xmin, xmax);
		drawAxis2D(p, state.pxm, scene, units[1], yaxislabel, yTicks[plot], scene.logY, ymin, ymax);
	}

	if (eq(xmin,xmax) || eq(ymin,ymax) || eq(zmin,zmax)) {
		LOG_WARNING("some of {x,y,z}{min,max} are equal: "<<xmin<<xmax<<","<<ymin<<ymax<<","<<zmin<<zmax);
		return;
	}
}

void drawAltRowsCols(QPainter &p, MyPlotterState &state, float xmin, float ymin) {
	const Plot *plot=state.plot;
	Scene &scene=state.scene;

	QColor color=QColor(Qt::gray);
	color.setAlpha(70);
	p.setPen(color);
	p.setBrush(QBrush(color));
	const int N=10;
	if (plot->altColumns>0) {
		for(int i=-N; i<N; i++) {
			const int v=int(floor(xmin)+i*plot->altColumns*2);
			const QPoint pt1=scene.doAll(QVector3D(v,0,0)).toPoint();
			const QPoint pt2=scene.doAll(QVector3D(v+plot->altColumns,0,0)).toPoint();
			p.drawRect(pt1.x(), 0, pt2.x()-pt1.x(), state.pxm.height());
		}
	}

	if (plot->altRows>0) {
		for(int i=-N; i<N; i++) {
			const int v=int(floor(ymin)+i*plot->altRows*2);
			const QPoint pt1=scene.doAll(QVector3D(0,v,0)).toPoint();
			const QPoint pt2=scene.doAll(QVector3D(0,v+plot->altRows,0)).toPoint();
			p.drawRect(0,pt1.y(), state.pxm.width(), pt2.y()-pt1.y());
		}
	}
}

void MyPlotter::repaint3D(AspectRatioPixmapLabel *lblImage) {
	MyPlotterState &state=states[lblImage];
	const Plot *plot=state.plot;
	Q_ASSERT(plot);
	Scene &scene=state.scene;
	QPainter p(&state.pxm);

	auto font=p.font();
	font.setFamily(FONT_FAMILY);
	p.setFont(font);

	const bool is2D=!scene.is3D;

	ObjectsList objList;

	// NOTE: vertHist-LogX: If we have a VertHist and logX, then we want to logX the individual histograms and not the whole plot.
	ValueRestorer<bool> logX(scene.logX); // WARNING: changing const-ness here!
	if (hasVertHist(state)) { logX.set(false); }

	SET_RANGE_VAR_MIN(xmin,0); SET_RANGE_VAR_MAX(xmax,0);
	SET_RANGE_VAR_MIN(ymin,1); SET_RANGE_VAR_MAX(ymax,1);
	SET_RANGE_VAR_MIN(zmin,2); SET_RANGE_VAR_MAX(zmax,2);

	if (eq(xmin,xmax) || xmax<xmin) { xmax=xmin+1; }
	if (eq(ymin,ymax) || ymax<ymin) { ymax=ymin+1; }
	if (eq(zmin,zmax) || zmax<zmin) { zmax=zmin+1; }
	if (is2D) { zmin=0; zmax=1; }

	setBoundaries(state); // called in finish()
	drawAxis(p, state, objList, xmin, xmax, ymin, ymax, zmin, zmax);
	if (is2D) {  drawAltRowsCols(p, state, xmin, ymin); }

	const int gridSize=plot->d3Mesh;
	const bool regrid=state.gridSize!=gridSize;
	const int real_title_height = opts.plotTitle ? TITLE_HEIGHT : 0;

	// Draw the points/data/stuff
	for(int serieIdx=0; serieIdx<state.series.size(); serieIdx++) {
		// We might take the if after deleting a trace. I don't know exactly why yet, but
		// this is a simple solution to fix it.
		if (state.traceIdx[serieIdx]>=plot->traces.size()) { continue; }

		const Serie &serie=state.series[serieIdx];
		const Trace &trace=*plot->traces[state.traceIdx[serieIdx]];

		if (!trace.style) { continue; }

		const auto &style=*trace.style;
		const QString str_color=style.get_opt_str(trace, "color", "");
		const int plotIndex=plot->set->stableColors ? plot->traceIdx(trace) : serieIdx;
		const QColor color=QColor(str_color.length() ? str_color : pick_color(plotIndex));

		p.setPen(Qt::black);
		if (serie.vertical.set) {
			const QPoint pt=scene.doAll(QVector3D(serie.vertical.value,0,0)).toPoint();
			p.drawLine(pt.x(), real_title_height, pt.x(), state.pxm.height());
		}
		if (serie.horizontal.set) {
			const QPoint pt=scene.doAll(QVector3D(0,serie.horizontal.value,0)).toPoint();
			p.drawLine(LEFT_GUTTER, pt.y(), state.pxm.width(), pt.y());
		}

		p.setPen(QPen(color));
		p.setBrush(QBrush(p.pen().color()));

		switch (style.style()) {
            case ETraceStyle::Line:
            case ETraceStyle::Histogram2D:
            case ETraceStyle::Points2D: case ETraceStyle::Points3D:
			case ETraceStyle::Violin:
            case ETraceStyle::VertHistogram: {
                TraceStyle *tracestyle=name_to_traceStyle(style.name());
                tracestyle->plot(p, state, serieIdx, objList);
                delete tracestyle;
                break;
            }
			default: LOG_WARNING("MyPlotter::repaint3D: MyPlotter cannot handle "<<style.name()<<" yet"); break;
		}

		if (plot->d3Grid) { // draw a mesh
			auto pos2idx=[gridSize](int ix, int iy) { return ix*(gridSize+1)+iy; };
			while (state.gridWeights.size()<=serieIdx) { state.gridWeights<<QList<double>(); }

			if (regrid || state.gridWeights[serieIdx].size()==0) {
				state.gridSize=gridSize;
				state.gridWeights[serieIdx].clear();
				const int norm=4;


				// We push all the weights here in some order. Below, we read them out in the same order.
				const float w=(xmax-xmin)/gridSize, h=(ymax-ymin)/gridSize;
				for(int ix=0; ix<=gridSize; ix++) {
					for(int iy=0; iy<=gridSize; iy++) {
						const float x=ix*w, y=iy*h;

						WeightedMean<double> wm;
						for(const Trace::Row &row : serie.rows) {
							const QVector3D pt=getPt(trace, row);
							const QVector2D pt1=mx4ToPt(scene.d3Scale*ptTomx4(pt)).toVector2D();
							const QVector2D pt2=mx4ToPt(scene.d3Scale*ptTomx4(QVector3D(x,y,pt.z()))).toVector2D();
							const QVector2D diff=pt2-pt1;
							const double dst=double(pow(pow(diff.x(),2)+pow(diff.y(),2),norm/2));
							const double w=1./std::max(1e-9, dst);
							wm.collect(double(pt.z()), w);
						}
						state.gridWeights[serieIdx]<<wm.mean;
						Q_ASSERT(state.gridWeights[serieIdx].size()-1==pos2idx(ix,iy));
					}
				}
			}

			const float w=(xmax-xmin)/gridSize, h=(ymax-ymin)/gridSize;
			for(int ix=0; ix<=gridSize; ix++) {
				QVector3D prevPtA=INVALID_VX, prevPtB=INVALID_VX;
				for(int iy=0; iy<=gridSize; iy++) {
					{ // Draw in first direction
						const float x=ix*w, y=iy*h;
						const int ptIdx=pos2idx(ix,iy);
						const float z=float(state.gridWeights[serieIdx][ptIdx]);

						zmin=std::min(zmin, z);
						zmax=std::max(zmax, z);
						QVector3D curPt=QVector3D(x,y,z);
						if (prevPtA!=INVALID_VX) { scene.drawLine(p, objList, curPt, prevPtA); }
						prevPtA=curPt;
					}
					{ // Draw in other direction, by switching x and y
						const float x=iy*w, y=ix*h;
						const int ptIdx=pos2idx(iy,ix);
						const float z=float(state.gridWeights[serieIdx][ptIdx]);

						zmin=std::min(zmin, z);
						zmax=std::max(zmax, z);
						QVector3D curPt=QVector3D(x,y,z);
						if (prevPtB!=INVALID_VX) { scene.drawLine(p, objList, curPt, prevPtB); }
						prevPtB=curPt;
					}
				}
			}
		}
	}

	if (plot->set->bbox) { // Draw bounding box indicators
		p.setPen(Qt::gray);
		p.setBrush(QBrush());
		scene.drawBox(p, objList, state.bbLB, state.bbUB);
	}

	if (opts.plotTitle){ // Draw title
		p.setFont(QFont(FONT_FAMILY,real_title_height));
		p.setPen(Qt::black);
		p.drawText(QRect(0,0,state.pxm.width(), real_title_height), state.apply_variables(plot->title)
			   , QTextOption(Qt::AlignmentFlag::AlignCenter|Qt::AlignmentFlag::AlignVCenter));

	}

	if (opts.plotLegend) { // Draw legend
		p.setFont(QFont(FONT_FAMILY, LEGEND_TEXT_HEIGHT));
		for(int serieIdx=0; serieIdx<state.series.size(); serieIdx++) {
			const int i=serieIdx;
			if (state.traceIdx[serieIdx]>=plot->traces.size()) { continue; }
			const Trace &trace=*plot->traces[state.traceIdx[serieIdx]];

			const int plotIndex=plot->set->stableColors ? plot->traceIdx(trace) : serieIdx;
			const auto *style=trace.style;
			const QString str_color=style ? style->get_opt_str(trace, "color", "") : "";
			const QColor color=QColor(str_color.length() ? str_color : pick_color(plot->set->stableColors ? plot->traceIdx(trace) : plotIndex));
			const QString title=trace.get_title(*state.override_vars);

			if (title=="-") { continue; }

			p.setPen(Qt::black);
			p.drawText(QRect(0,real_title_height+i*LEGEND_TEXT_HEIGHT,state.pxm.width()-50,LEGEND_TEXT_HEIGHT), title,QTextOption(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignVCenter));
			if (state.selSeries==i) {
				p.setPen(QPen(color,2));
				p.setBrush(QBrush());
				p.drawRect(QRect(state.pxm.width()-49,real_title_height+i*LEGEND_TEXT_HEIGHT,state.pxm.width(),LEGEND_TEXT_HEIGHT));
			}
			p.setPen(color);
			p.setBrush(QBrush(color));
			p.drawLine(state.pxm.width()-49,real_title_height+i*LEGEND_TEXT_HEIGHT+LEGEND_TEXT_HEIGHT/2,state.pxm.width(),real_title_height+i*LEGEND_TEXT_HEIGHT+LEGEND_TEXT_HEIGHT/2);
			scene.drawSymbol2D(p, plotIndex, QPoint(state.pxm.width()-49+20, real_title_height+i*LEGEND_TEXT_HEIGHT+LEGEND_TEXT_HEIGHT/2), 10, color);
		}
	}

    scene.drawObjects(p, objList); // draw the scene

	if (state.selPoint!=INVALID_VX) { // draw lines perpendicular to the axes if a point is selected.
		p.setPen(Qt::black);
		QVector3D pt=state.selPoint;
        pt[1] *= 1.1;
		scene.drawSphere(p, pt, 10);
		scene.drawText(p, pt, state.selInfo, 15);
		scene.drawLine(p, objList, QVector3D(pt.x(), pt.y(), 0), pt);
		scene.drawLine(p, objList, QVector3D(pt.x(), pt.y(), 0), QVector3D(pt.x(), 0, 0));
		scene.drawLine(p, objList, QVector3D(pt.x(), pt.y(), 0), QVector3D(0, pt.y(), 0));
	}


	lblImage->setPixmap(state.pxm);
}

QString rowToString(const MyPlotterState &state, const Trace::Row &row) {
	return state.scene.is3D
        ? QString("(%1,%2,%3)").arg(row[0].toString(),row[1].toString(),row[2].toString())
        : QString("(%1,%2)").arg(row[0].toString(),row[1].toString());
}

int clicked_on_legend(MyPlotterState &state, const PlotOptions &opts, QVector2D mouse) {
	QPainter p(&state.pxm);
	const Plot *plot=state.plot;
	p.setFont(QFont(FONT_FAMILY,LEGEND_TEXT_HEIGHT));
	const int real_title_height = opts.plotTitle ? TITLE_HEIGHT : 0;
	for(int serieIdx=0; serieIdx<state.series.size(); serieIdx++) {
		const int i=serieIdx;
		if (state.traceIdx[serieIdx]>=plot->traces.size()) { continue; }
		const Trace &trace=*plot->traces[state.traceIdx[serieIdx]];
		const QString title=trace.get_title(*state.override_vars);
		if (title=="-") { continue; }

		QRect tRect=p.fontMetrics().boundingRect(title);
		tRect=tRect.translated(state.pxm.width()-50-tRect.width(),real_title_height+(i+1)*LEGEND_TEXT_HEIGHT);
		if (tRect.contains(mouse.toPoint())) { return serieIdx; }
	}
	return -1;
}

bool MyPlotter::eventFilter(QObject *obj, QEvent *event) {
	// NOTE: eventFilter is not really called through Qt's mechanism, but from AspectRatioPixmapLabel's event.
	// Thus it does not matter what we return here ...

	AspectRatioPixmapLabel *label=dynamic_cast<AspectRatioPixmapLabel *>(obj);
	if (!states.contains(label)) { return QObject::eventFilter(obj, event); }

	MyPlotterState &state=states[label];

    QMouseEvent *mouseEvent=dynamic_cast<QMouseEvent *>(event);
    QWheelEvent *wheelEvent=dynamic_cast<QWheelEvent *>(event);
    if (mouseEvent) {
        const bool shiftPressed=mouseEvent->modifiers().testFlag(Qt::ShiftModifier);
        const bool leftButtonPressed=mouseEvent->buttons() & Qt::LeftButton;
        const bool midButtonPressed=mouseEvent->buttons() & Qt::MiddleButton;

		const bool actionSelect=leftButtonPressed;
		const bool actionMove=midButtonPressed || (leftButtonPressed && shiftPressed);

        QVector2D mouse(mouseEvent->pos());
		if (state.pxm.height()<label->height()) {
			// If the pixmap is smaller, then it is centered vertically, skewing our y pos.
			// The label is left-aligned, so horizontally it does not happen.
			mouse.setY(mouse.y()-(label->height()-state.pxm.height())/2);
		}


		switch (event->type()) {
		case QEvent::MouseMove:
			if (actionMove) {
				tmpMouseOffset=mouse.toPoint()-midButtonMousePos;
				for(auto *win:MainWindow::instances) {
					for(auto *img : win->images) { repaint(img); }
				}
				setBoundaries(state);
			}
			break;
		case QEvent::MouseButtonPress:
			if (actionMove) {
				midButtonMousePos=mouse.toPoint();
				tmpMouseOffset=QPoint();
				repaint(label);
			} else if (actionSelect) {
				const int legend_entry=opts.plotLegend ? clicked_on_legend(state, opts, mouse) : -1;
				if (legend_entry>=0) {
					state.selInfo="";
					state.selSeries=legend_entry;
				} else {
					QVector3D closest; unsigned bestDst=unsigned(-1);
					state.selInfo="";
					state.selSeries=-1;
					for(int serieIdx=0; serieIdx<state.series.size(); serieIdx++) {
						const Serie &serie=state.series[serieIdx];
						if (!serie.trace || !serie.trace->style) { continue; }

						const auto &style=*serie.trace->style;
						switch (style.style()) {
							case ETraceStyle::Histogram2D:
							case ETraceStyle::Points2D: case ETraceStyle::Points3D:
							case ETraceStyle::Violin:
							case ETraceStyle::VertHistogram:
							name_to_traceStyle(style.name())->handleMouseClick(state, serieIdx, mouse, closest, bestDst);
							break;
							default: LOG_WARNING("WARNING: MyPlotter cannot handle "<<style.name()<<" yet");
						}
					}
					if (bestDst<15) {
						state.selPoint=closest;
					} else {
						state.selPoint=INVALID_VX;
						state.selSeries=-1;
					}
				}
				repaint(label);
			}
			break;
		case QEvent::MouseButtonRelease: {
			if (true) { // NOTE: actionMove will not be true here anymore!
				mouseOffset+=tmpMouseOffset;
				tmpMouseOffset=midButtonMousePos=QPoint();
			}
			break;
		}
		case QEvent::Resize:
			repaint(label);
			break;
		default: break;
		}
    } else if (wheelEvent) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        QVector2D mouse(wheelEvent->pos());
#else
        QVector2D mouse(wheelEvent->position());
#endif
        for(auto &label2 : states.keys()) {
//            const float dy=we->pixelDelta().y();
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            const double delta = wheelEvent->delta();
#else
            const double delta = wheelEvent->angleDelta().y();
#endif
            const float s = 1.f + delta/1000.0;
            MyPlotterState &state2 = states[label2];
            state2.scene.zoom *= s;
            const QPoint midPoint(label->width()/2, label->height()/2);
            state2.scene.d3SetVp(state2.scene.tf(QVector3D(mouse.x(), mouse.y(), 0)));
            qDebug()<<midPoint<<"\n\t"<<(mouse.toPoint()-midPoint)<<"\n\t"<<mouseOffset;
//            state2.scene.d2Scale(mouse.toPoint().x(), mouse.toPoint().y(), s, s);
			setBoundaries(state2);
//            state2.scene.d3SetVp(QVector3D(axes[0].x()/2, axes[1].y()/2, 0));
            repaint(label2);
		}
	}
	return QObject::eventFilter(obj, event);
}


#include "myplotter_scene.h"

QString MyPlotterState::apply_variables(const QString &str) const { return plot->apply_variables(str, *override_vars); }
QString MyPlotterState::apply_variables(const QString &str, const Trace &trace) const { return trace.apply_variables(str, *override_vars); }
