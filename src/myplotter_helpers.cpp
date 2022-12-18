#include "myplotter_helpers.h"

QMap<const Plot*,TickMap> xTicks, yTicks, zTicks;


float goodValue(const QString &spec, const float value, const float lo, const float hi) {
    if (spec=="" || spec=="*") {
        return lo-(hi-lo)*0.1f;
    } else if (spec.mid(0,2)=="*/") {
        const float rounder=spec.mid(2).toFloat();
        const float ret1=lo-(hi-lo)*0.1f;
        const float ret=float(ceil(ret1/rounder))*rounder;
        return ret;
    } else {
        return value;
    }
}

Mx4 ptTomx4(const QVector3D &pt) {
    Mx4 ret; ret.fill(0);
    for(int r=0; r<3; r++) { ret(r,0)=pt[r]; }
    ret(3,0)=1;
    return ret;
}

Mx4 rotX(float theta) {
    Mx4 ret; ret.setToIdentity();
    ret(1,1)=cos(theta);
    ret(1,2)=-sin(theta);
    ret(2,1)=sin(theta);
    ret(2,2)=cos(theta);
    return ret;
}

Mx4 rotY(float theta) {
    Mx4 ret; ret.setToIdentity();
    ret(0,0)=cos(theta);
    ret(0,2)=sin(theta);
    ret(2,0)=-sin(theta);
    ret(2,2)=cos(theta);
    return ret;
}

Mx4 rotZ(float theta) {
    Mx4 ret; ret.setToIdentity();
    ret(0,0)=cos(theta);
    ret(0,1)=-sin(theta);
    ret(1,0)=sin(theta);
    ret(1,1)=cos(theta);
    return ret;
}

Mx4 scale3D(float sx, float sy, float sz) {
    Mx4 ret; ret.setToIdentity();
    ret(0,0)=sx;
    ret(1,1)=sy;
    ret(2,2)=sz;
    return ret;
}

Mx4 translate3D(float x, float y, float z) {
    Mx4 ret;
    ret.setToIdentity();
    ret(0,3)=x;
    ret(1,3)=y;
    ret(2,3)=z;
    return ret;
}

float mypow10(const float x) { return pow(10.f, x); }
float mylog10(const float x) {
    if (eq(x,0)) {
        return x>=0 ? log10(LOG_ZERO) : -log10(LOG_ZERO);
    }
    return x<0 ? -log10(-x) : log10(x);
}

QPoint maxPW(const QPoint &p1, const QPoint &p2) { return QPoint(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y())); }

QPoint minPW(const QPoint &p1, const QPoint &p2) { return QPoint(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y())); }

QVector3D _limitPW(float (*f)(float, float), const QVector3D &p1, const QVector3D &p2) {
    if (p1==INVALID_VX) { return p2; }
    if (p2==INVALID_VX) { return p1; }
    return QVector3D(f(p1[0], p2[0]), f(p1[1], p2[1]), f(p1[2], p2[2]));
}

QVector3D getPt(const Trace &trace, const Trace::Row &row) {
    if (row.size()<2) { return INVALID_VX; }
    const float x=trace.xtic ? getTick(xTicks[trace.plot], row[0].toString()) : row[0].toFloat();
    const float y=trace.ytic ? getTick(yTicks[trace.plot], row[1].toString()) : row[1].toFloat();
    const float z=row.size()>=3 ? (trace.ztic ? getTick(zTicks[trace.plot], row[2].toString()) : row[2].toFloat()) : 0;
    return QVector3D(x,y,z);
}




float getTick(TickMap &tickMap, const QString &key) {
    for(int i=0; i<tickMap.size(); i++) {
        if (tickMap[i]==key) { return i; }
    }
    tickMap<<key;
    return tickMap.size()-1;
}


QString toString(const QVector3D &v) { return QString("(%1,%2,%3)").arg(double(v[0])).arg(double(v[1])).arg(double(v[2])); }
