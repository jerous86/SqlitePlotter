#pragma once

#include <QPainter>
#include <QFont>
#include <QVector2D>

#include "plot_myplotter.h"
#include "myplotter_helpers.h"

bool eq(float l, float r);

// use*[0:min-x, 1:max-x, 2:min-y, 3:max-y]
void Scene::set2DTransformation(QPixmap &pxm, const QList<QVector3D> &axes, const QList<Serie> &series, QVector3D llimit, QVector3D ulimit,
	QVector<bool> useAxes, QVector<bool> useSeries, QVector<bool> useLimits
) {
	d2Init();

	QPoint pmin(INT_MAX,INT_MAX), pmax(INT_MIN,INT_MIN);

	#define UPDATE_MIN_X(NEW_PT)	pmin.setX(std::min(pmin.x(), NEW_PT.x()))
	#define UPDATE_MAX_X(NEW_PT)	pmax.setX(std::max(pmax.x(), NEW_PT.x()))
	#define UPDATE_MIN_Y(NEW_PT)	pmin.setY(std::min(pmin.y(), NEW_PT.y()))
	#define UPDATE_MAX_Y(NEW_PT)	pmax.setY(std::max(pmax.y(), NEW_PT.y()))

	const auto lp=doAll(ulimit).toPoint();
	const auto up=doAll(llimit).toPoint();
	if (useLimits[0]) { UPDATE_MIN_X(lp); }
	if (useLimits[1]) { UPDATE_MAX_X(up); }
	if (useLimits[2]) { UPDATE_MIN_Y(lp); }
	if (useLimits[3]) { UPDATE_MAX_Y(up); }

	if (useSeries[0] || useSeries[1] || useSeries[2] || useSeries[4]) {
		if (series.size()>0 && series[0].rows.size()>0) {
			QPoint pti=doAll(getPt(*series[0].trace, series[0].rows[0])).toPoint();
			if (useSeries[0]) { UPDATE_MIN_X(pti); }
			if (useSeries[1]) { UPDATE_MAX_X(pti); }
			if (useSeries[2]) { UPDATE_MIN_Y(pti); }
			if (useSeries[3]) { UPDATE_MAX_Y(pti); }
		}
		// We want to fill the image, so now we determine how much we can scale up/down the axis, by plotting all points
		// and determining the maxima.
		// For now we work very inefficiently by plotting points twice: here, and later on when drawing them.
		for(const Serie &serie:series) {
			for(const Trace::Row &row : serie.rows) {
				const QVector3D pt=getPt(*serie.trace, row);
				const QPoint pti=doAll(pt).toPoint();
				if (useSeries[0]) { UPDATE_MIN_X(pti); }
				if (useSeries[1]) { UPDATE_MAX_X(pti); }
				if (useSeries[2]) { UPDATE_MIN_Y(pti); }
				if (useSeries[3]) { UPDATE_MAX_Y(pti); }
			}
		}
	}

	for(const QVector3D &ax:axes) {
		const QPoint pti=doAll(ax).toPoint();
		if (useAxes[0]) { UPDATE_MIN_X(pti); }
		if (useAxes[1]) { UPDATE_MAX_X(pti); }
		if (useAxes[2]) { UPDATE_MIN_Y(pti); }
		if (useAxes[3]) { UPDATE_MAX_Y(pti); }
	}

	const float
		scalex=float(pxm.width())/float(pmax.x()-pmin.x()),
		scaley=float(pxm.height())/float(pmax.y()-pmin.y());

//	qDebug()<<"set2DTransformation"<<useAxes<<useSeries<<useLimits;
//	qDebug()<<"set2DTransformation"<<pmin<<pmax;

    d2Scale(0,0,scalex, scaley);
	d2Translate(int(-pmin.x()*scalex), int(-pmin.y()*scaley));
    d2Scale(0,0,0.8f,0.8f);
	d2Translate(int(pxm.width()*0.2/2), int(pxm.height()*0.2/2));
}


QVector3D Scene::tf(const QVector3D &pt_) const {
    QVector3D pt=pt_;
    if (logX) { pt[0]=mylog10(pt[0]); }
    if (logY) { pt[1]=mylog10(pt[1]); }
    if (logZ) { pt[2]=mylog10(pt[2]); }

    Mx4 mx=ptTomx4(pt);
    mx(1,0)*=-1; // convert to left-handed, so it matches with gnuplot
    Mx4 vpInv=d3Vp; for(int i=0; i<3; i++) { vpInv(i,3)*=-1; } // translate to origin
    const Mx4 res=vpInv * d3Rot * d3Scale * mx * d3Vp;
    return mx4ToPt(res);
}

QVector2D Scene::project(const QVector3D &pt) const { return pt.toVector2D(); }

QVector2D Scene::viewport(const QVector2D &pt) const {
    Mx3 m; m.fill(0);
    m(0,0)=pt.x(); m(1,0)=pt.y(); m(2,0)=1;
    const Mx3 res=d2Tf*m;
    return QVector2D(res(0,0), res(1,0));
}

QVector2D Scene::doAll(const QVector3D &pt) const { return viewport(project(tf(pt))); }

ObjectsList::Object newObject(QPainter &p) {
    ObjectsList::Object result;
    result.pen=p.pen();
    result.brush=p.brush();
    result.pts=QList<QVector3D>();
    return result;
}

void Scene::drawLine(QPainter &p, ObjectsList &objList, QVector3D pt1, QVector3D pt2) const {
    ObjectsList::Object obj=newObject(p);
    obj.pts<<pt1<<pt2;
    objList.addObject(obj);
}

QVector3D centerOfMass(const ObjectsList::Object &obj) {
	QVector3D ret;
	for(auto &x:obj.pts) {
		ret+=x;
	}
	ret*=1./obj.pts.size();
	return ret;
}

void Scene::drawObjects(QPainter &p, const ObjectsList &objList) const {
	using Object = ObjectsList::Object;
	// First sort by depth. As depth, we use the center of mass.
	// We assume here that all objects are convex.
	QList<Object> tfObjs;
    for(const Object &obj:objList.objs) {
		Object tfObj=obj;
		tfObj.pts.clear();
        for(const QVector3D &x:obj.pts) { tfObj.pts<<tf(x); }
		tfObjs<<tfObj;
	}

	std::sort(tfObjs.begin(), tfObjs.end(), [](const Object &l, const Object &r) { return centerOfMass(l).z()<centerOfMass(r).z(); });

	for(auto &obj : tfObjs) {
		QPolygon projObj;
		for(auto &x:obj.pts) { projObj<<viewport(project(x)).toPoint(); }
		p.setPen(obj.pen);
		p.setBrush(obj.brush);
		p.drawPolygon(projObj);
	}
}

void Scene::drawBox(QPainter &p, ObjectsList &objList, QVector3D l, QVector3D u) const {
#define V(x,y,z)    (QVector3D(bool(x)?l[0]:u[0], bool(y)?l[1]:u[1], bool(z)?l[2]:u[2]))
    const QList<uint8_t> xs=QList<uint8_t>()<<0x0/*0b00*/<<0x2/*0b10*/<<0x3/*0b11*/<<0x1/*0b01*/<<0x0;
    ObjectsList::Object obj=newObject(p);
    { obj.pts.clear(); for(auto x:xs) { obj.pts<<V(x&1, x&2, 0); } objList.addObject(obj); }
    { obj.pts.clear(); for(auto x:xs) { obj.pts<<V(x&1, x&2, 1); } objList.addObject(obj); }
    { obj.pts.clear(); for(auto x:xs) { obj.pts<<V(x&1, 0, x&2); } objList.addObject(obj); }
    { obj.pts.clear(); for(auto x:xs) { obj.pts<<V(x&1, 1, x&2); } objList.addObject(obj); }
    { obj.pts.clear(); for(auto x:xs) { obj.pts<<V(0, x&1, x&2); } objList.addObject(obj); }
    { obj.pts.clear(); for(auto x:xs) { obj.pts<<V(1, x&1, x&2); } objList.addObject(obj); }
}

void Scene::drawFilledPolygon(QPainter &p, ObjectsList &objList, const QList<QVector3D> &pts) const {
    ObjectsList::Object result;
    result.pen=p.pen();
    result.brush=p.brush();
    result.pts=pts;

    objList.addObject(result);
}
void Scene::drawFilledRect(QPainter &p, ObjectsList &objList, QVector3D pt1, QVector3D size) const {
	Q_ASSERT(eq(size[2],0));
	QList<QVector3D> pts;
	pts << pt1 << pt1+QVector3D(0,size[1],0) <<pt1+size <<pt1+QVector3D(size[0],0,0);
	drawFilledPolygon(p, objList, pts);
}

void Scene::drawText(QPainter &p, QVector3D pt, const QString &line, int ps) const {
    const QPoint pti=doAll(pt).toPoint();
    QFont font=p.font();
    font.setFamily(FONT_FAMILY);
    font.setPointSize(ps);
    p.setFont(font);
    p.drawText(pti, line);
}

void Scene::drawSphere(QPainter &p, QVector3D pt, float r) const {
    const QPoint pti=doAll(pt).toPoint();
//    const QPoint ptr=doAll(QVector3D(r,0,0)).toPoint();
    drawSphere2D(p, pti, r);
}
void Scene::drawSphere2D(QPainter &p, QPoint pti, float r) const {
    const int ri=int(r);
    p.drawEllipse(pti.x()-ri/2,pti.y()-ri/2,ri,ri);
}

void Scene::drawSymbol(QPainter &p, int i, QVector3D pt, float r, QColor color) const {
    drawSymbol2D(p, i, doAll(pt).toPoint(), r, color);
}
void Scene::drawSymbol2D(QPainter &p, int i, QPoint pt, float r, QColor color) const {
    const int R2=int(r/2);
    const int N=5;
    p.setBrush(int(i/N)%2==0 ? QBrush(color) : QBrush());
    p.setPen(QPen(color, 2));
    if (i<0) {
    } else if (i%N==0) { //square
        const QPolygon poly=QPolygon()<<pt+QPoint(-R2,-R2)<<pt+QPoint(-R2,R2)<<pt+QPoint(R2,R2)<<pt+QPoint(R2,-R2);
        p.drawPolygon(poly);
    } else if (i%N==1) { // regular triangle
        const QPolygon poly=QPolygon()<<pt+QPoint(0,-R2)<<pt+QPoint(R2,R2)<<pt+QPoint(-R2,R2);
        p.drawPolygon(poly);
    } else if (i%N==2) { // inverted triangle
        const QPolygon poly=QPolygon()<<pt+QPoint(0,R2)<<pt+QPoint(R2,-R2)<<pt+QPoint(-R2,-R2);
        p.drawPolygon(poly);
    } else if (i%N==3) { // diamond
        const QPolygon poly=QPolygon()<<pt+QPoint(0,-R2)<<pt+QPoint(-R2,0)<<pt+QPoint(0,R2)<<pt+QPoint(R2,0);
        p.drawPolygon(poly);
    } else if (i%N==4) { drawSphere2D(p,pt,r);
    } else { Q_ASSERT(false); // Update N to the correct amount of symbols!
    }
}

void Scene::d3Init() {
    d3SetVp(QVector3D());
    d3Rot.setToIdentity();
    d3Scale.setToIdentity();
}

void Scene::d3SetVp(QVector3D vp_) {
    d3Vp.setToIdentity();
    for(int i=0; i<3; i++) { d3Vp(i,3)=vp_[i]; }
}

void Scene::d2Init() {
    d2Tf.setToIdentity();
}

void Scene::d2Translate(int tx, int ty) {
    Mx3 m;
    m.setToIdentity();
    m(0,2) = tx;
    m(1,2) = ty;
    d2Tf = m*d2Tf;
}

void Scene::d2Scale(int cx, int cy, float sx, float sy) {
    Mx3 m;
    d2Translate(-cx, -cy);
    m.setToIdentity();
    m(0,0) = sx*zoom;
    m(1,1) = sy*zoom;
    d2Tf = m*d2Tf;
    d2Translate(cx, cy);
}
