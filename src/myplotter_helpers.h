#pragma once

#include <math.h>
#include <cmath>

#include <QString>
#include <QMap>
#include <QVector3D>

#include "plot_myplotter.h"

// This class stores the value at construction, and restores this value at destruction.
template<class T>
struct ValueRestorer {
    explicit ValueRestorer(T &cur): curValue(cur), orig(cur) { }
    ~ValueRestorer() { curValue=orig; }
    void set(const T &v) { curValue=v; }
    T &get() { return curValue; }
    const T origValue() const { return orig; }

private:
    T &curValue;
    const T orig;
};


inline float deg2rad(float deg) { return deg/360*2*float(M_PI); }
inline bool eq(float l, float r) { return fabs(double(l)-double(r))<=1e-10; }


float goodValue(const QString &spec, const float value, const float lo, const float hi);

Mx4 ptTomx4(const QVector3D &pt);
inline QVector3D mx4ToPt(const Mx4 &mx) { return QVector3D(mx(0,0), mx(1,0), mx(2,0)); }
Mx4 rotX(float theta);
Mx4 rotY(float theta);
Mx4 rotZ(float theta);
Mx4 scale3D(float sx, float sy, float sz);
Mx4 translate3D(float x, float y, float z);


typedef QList<QString> TickMap;
extern QMap<const Plot*,TickMap> xTicks, yTicks, zTicks;
float getTick(TickMap &tickMap, const QString &key);




// min, max pointwise to get the bounding box
QPoint maxPW(const QPoint &p1, const QPoint &p2);
QPoint minPW(const QPoint &p1, const QPoint &p2);

inline float maxf(float l, float r) { return std::max(l,r); }
inline float minf(float l, float r) { return std::min(l,r); }
QVector3D _limitPW(float (*f)(float,float), const QVector3D &p1, const QVector3D &p2);

inline QVector3D maxPW(const QVector3D &p1, const QVector3D &p2) { return _limitPW(maxf, p1, p2); }
inline QVector3D minPW(const QVector3D &p1, const QVector3D &p2) { return _limitPW(minf, p1, p2); }

const MapQStringQString EmptyMap;
QVector3D getPt(const Trace &trace, const Trace::Row &row);


//template<class T> T id(const T &x) { return x; }
#define LOG_ZERO 1e-99
float mylog10(const float x);
float mypow10(const float x);

QString toString(const QVector3D &v);
