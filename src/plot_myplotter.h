#pragma once

class QLabel;
class QPixmap;
class QPainter;
#include <QJSEngine>
#include <QVector3D>
#include <QGenericMatrix>
#include <QPixmap>

class AspectRatioPixmapLabel;

#define FONT_FAMILY	"Helvetica"
const float DEFAULT_HISTOGRAM_BAR_WIDTH=0.4f;

#include "plot.h"
#include "plot_task.h"

static const QVector3D INVALID_VX(1e9f,1e9f+1.f,1e9f+2.f);

typedef QGenericMatrix<4,4,float> Mx4;
typedef QGenericMatrix<3,3,float> Mx3;
typedef QGenericMatrix<2,2,float> Mx2;

float deg2rad(float deg);
Mx4 ptTomx4(const QVector3D &pt);
Mx4 rotX(float theta_rad);
Mx4 rotY(float theta_rad);
Mx4 rotZ(float theta_rad);
Mx4 scale3D(float sx, float sy, float sz);
Mx4 translate3D(float x, float y, float z);


QVector3D maxPW(const QVector3D &p1, const QVector3D &p2);
QVector3D minPW(const QVector3D &p1, const QVector3D &p2);
QPoint maxPW(const QPoint &p1, const QPoint &p2);
QPoint minPW(const QPoint &p1, const QPoint &p2);

QVector3D getPt(const Trace &trace, const Trace::Row &row);

//template<class T> T id(const T &x) { return x; }
template<class T> T id(const T x) { return x; }
float mylog10(const float x);
float mypow10(const float x);


Mx4 ptTomx4(const QVector3D &pt);
QVector3D mx4ToPt(const Mx4 &mx);


template<class T>
struct Option {
	bool set=false;
	T value;

	void enable(const T &value_) { set=true; value=value_; }
	void disable() { set=false; }
};

struct Serie {
	// The data for plotting a point
	//    QList<QVector3D> pts;
	const Trace *trace=nullptr;
	QStringList columnNames; // the names for $rows
	Trace::Rows rows; // the sqlite data
	Option<float> vertical, horizontal;

	int colIndex(QString columnName) const { return columnNames.indexOf(columnName); }
};

struct ZBuffer {
	ZBuffer(QSize size_): size(size_) {
		buf.resize(size.width()*size.height());
		buf.fill(-1e99);
	}
	QVector<float> buf;
	const QSize size;

	void set(QPoint &p, float value) { buf[zbufferI(p)]=value; }
	float get(QPoint &p) { return buf[zbufferI(p)]; }
	int zbufferI(QPoint p) const { return (p.y())*(size.width())+(p.x()); }
};

struct ObjectsList {
	struct Object {
		QPen pen;
		QBrush brush;
		QList<QVector3D> pts;
	};

	QList<Object> objs;

	void addObject(const Object &obj) {
		objs<<obj;
	}
};

struct Scene {
	Mx4 d3Vp; // where is the camera looking at?
	Mx4 d3Rot;
	Mx4 d3Scale;
	Mx3 d2Tf;
	float zoom=1.;

	bool is3D;
	bool logX, logY, logZ;

	void set2DTransformation(QPixmap &pxm, const QList<QVector3D> &axes, const QList<Serie> &series, QVector3D llimit, QVector3D ulimit, QVector<bool> useAxes, QVector<bool> useSeries, QVector<bool> useLimits);

	QVector3D tf(const QVector3D &pt) const;
	QVector2D project(const QVector3D &pt) const;
	QVector2D viewport(const QVector2D &pt) const;
	QVector2D doAll(const QVector3D &pt) const;

    void drawObjects(QPainter &p, const ObjectsList &objList) const;

	void drawLine(QPainter &p, ObjectsList &objList, QVector3D pt1, QVector3D pt2) const;
	void drawBox(QPainter &p, ObjectsList &objList, QVector3D l, QVector3D u) const;
	void drawFilledPolygon(QPainter &p, ObjectsList &objList, const QList<QVector3D> &pts) const;
	void drawFilledRect(QPainter &p, ObjectsList &objList, QVector3D pt1, QVector3D size) const;

	void drawText(QPainter &p, QVector3D pt, const QString &line, int ps=7) const;
	void drawSphere(QPainter &p, QVector3D pt, float r) const;
	void drawSphere2D(QPainter &p, QPoint pti, float r) const;
	void drawSymbol(QPainter &p, int i, QVector3D pt, float r, QColor) const;
	void drawSymbol2D(QPainter &p, int i, QPoint pt, float r, QColor color) const;

	void d3Init();
	void d3SetVp(QVector3D d3Vp);

	void d2Init();
	void d2Translate(int tx, int ty);
    void d2Scale(int cx, int cy, float sx, float sy);

public:
};

float goodValue(const QString &spec, const float value, const float lo, const float hi);

#define SET_RANGE_VAR_MIN(VAR,DIM)  float VAR=goodValue(plot->VAR, plot->VAR.toFloat(), state.bbLB[DIM], state.bbUB[DIM])
#define SET_RANGE_VAR_MAX(VAR,DIM)  float VAR=goodValue(plot->VAR, plot->VAR.toFloat(), state.bbUB[DIM], state.bbLB[DIM])

struct MyPlotterState {
	MyPlotter *myPlotter;
	const Plot *plot=nullptr;
	const MapQStringQString *override_vars=nullptr;
	QPixmap pxm;

	QString apply_variables(const QString &str) const;
	QString apply_variables(const QString &str, const Trace &trace) const;

	QList<Serie> series;
	QList<int> traceIdx;
	QVector3D selPoint=INVALID_VX;
	QString selInfo;
	int selSeries=-1;
	Scene scene;
	QVector3D bbLB, bbUB; // bounding boxes of non-transformed points. Updated when adding data

	int gridSize=-1;
	QList<QList<double>> gridWeights;
};

struct MyPlotter: public QObject {
	Q_OBJECT
public:
	MyPlotter();
	virtual ~MyPlotter() override;

	void init(const Plot &plot, AspectRatioPixmapLabel *lblImage, const MapQStringQString &override_vars);
	// Returns all the columns that are required to plot this trace successfully.
	QStringList requiredColumns(const Trace &trace) const;
	void addData(const Trace &trace, const QStringList &columns, const Trace::Rows &rows);
	void finish();


	// Called when data has changed -- used to update grid
	static void invalidateCache(AspectRatioPixmapLabel *l);
	static bool isUsed(const AspectRatioPixmapLabel *l);
	static MyPlotter *getPlotter(const AspectRatioPixmapLabel *l);
	QPoint midButtonMousePos, mouseOffset, tmpMouseOffset;

    PlotOptions opts;

	static QMap<AspectRatioPixmapLabel *, MyPlotterState> states;
	void repaint(AspectRatioPixmapLabel *lblImage);
	void repaint3D(AspectRatioPixmapLabel *lblImage);

	QJSEngine scriptEngine;
private:
	bool eventFilter(QObject *obj, QEvent *event) override;



	AspectRatioPixmapLabel *curImg=nullptr;
	void setBoundaries(MyPlotterState &state);
};

