#include "aspectratiopixmaplabel.h"

#include <math.h>
#include <cmath>

#include <QPainter>
#include <QGenericMatrix>

#include "mainwindow.h"
#include "plot_myplotter.h" // for the 3D stuff
#include "myplotter_helpers.h"

// Adapted from: https://stackoverflow.com/a/22618496

AspectRatioPixmapLabel::AspectRatioPixmapLabel(bool keepAR, QWidget *parent) : QLabel(parent) {
	setMinimumSize(1,1);
	setKeepAspectRatio(keepAR);
	setMouseTracking(true);
	_showCH=true;
}

int AspectRatioPixmapLabel::heightForWidth( int width_ ) const {
	if (pix.isNull()) { return QLabel::heightForWidth(width_); }

	if (_keepAR) {
		const int ret=std::min(height(), width_);
		return ret;
	} else {
		return QLabel::heightForWidth(width_);
	}
}

QSize AspectRatioPixmapLabel::sizeHint() const {
	if (_keepAR) {
		const int w = this->width();
		const int s=heightForWidth(w);
		const QSize ret=QSize(s,s);
		return ret;
	} else {
		return QLabel::sizeHint();
	}
}

void AspectRatioPixmapLabel::setKeepAspectRatio(bool keep) {
	_keepAR=keep;
	if (keep) {
		//        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
		setScaledContents(false);
	} else {
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
		setScaledContents(true);
	}

	doIt();
}

void AspectRatioPixmapLabel::doIt() {
	if (pix.isNull()) { return; }
	QLabel::setPixmap(_keepAR ? scaledPixmap() : pix);
}

QPixmap AspectRatioPixmapLabel::scaledPixmap() const {
	const int x=sizeHint().width();
	return pix.scaled(QSize(x,x), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
void AspectRatioPixmapLabel::resizeEvent(QResizeEvent *e) {
	(void) e;
	doIt();
}

void AspectRatioPixmapLabel::setCustomPixmap(const QPixmap & p) {
	setPixmap(p);
	pix = p;
	doIt();
}


static QPointF ptNull=QPointF(-1.123,-1.321);
static QPointF pt0=QPointF(0,0);
static QPointF ch1; // crosshair
static QPointF ch2; // rectangle "crosshair"
static QPointF ch2MoveStartOffset; // if moving rectangle -- starting offset
static QPointF ch2Offset; // offset to add

typedef QGenericMatrix<3,3,double> Mx;
void project3d(QPainter &p, int x, int y, const Mx4 &tf) {
	const Mx4 ptX=tf * ptTomx4(QVector3D(1000,0,0));
	const Mx4 ptY=tf * ptTomx4(QVector3D(0,100,0));
	const Mx4 ptZ=tf * ptTomx4(QVector3D(0,0,100));

	// isometric projection, so we can just ignore pt(2,0).
	p.drawLine(int(x-ptX(0,0)),int(y-ptX(1,0)), int(x+ptX(0,0)),int(y+ptX(1,0)));
	p.drawLine(int(x-ptY(0,0)),int(y-ptY(1,0)), int(x+ptY(0,0)),int(y+ptY(1,0)));
	p.drawLine(int(x-ptZ(0,0)),int(y-ptZ(1,0)), int(x+ptZ(0,0)),int(y+ptZ(1,0)));
}

void AspectRatioPixmapLabel::paintEvent(QPaintEvent *e) {
	QLabel::paintEvent(e);
	if (_showCH) {
		QPainter p(this);

		const int w(width()); const int h(height());
		const int &x1=int((ch1.x()+ch2Offset.x())*w), &y1=int((ch1.y()+ch2Offset.y())*h);
		const int &x2=int((ch2.x()+ch2Offset.x())*w), &y2=int((ch2.y()+ch2Offset.y())*h);

		const bool p1=x1>0 && y1>0;
		const bool p2=x2>=0 && y2>=0;

		if (p1) {
			p.setPen(p2 ? Qt::gray : Qt::black);
			p.setOpacity(1);

			p.drawLine(x1, 0, x1, h);
			p.drawLine(0, y1, w, y1);
		}

		if (p2) {
			p.setPen(Qt::gray);
			p.setOpacity(1);

			p.drawLine(x2, 0, x2, h);
			p.drawLine(0, y2, w, y2);

			QColor fill=QColor(Qt::yellow).lighter();
			fill.setAlpha(80);
			p.fillRect(QRect(x1,y1, x2-x1,y2-y1), fill);
		}

		if (plot_type=="splot") {
			if (p1) {
				p.setPen(Qt::green);
				p.setOpacity(1);

				int extraD3XRot=0, extraD3ZRot=0;
				MyPlotter *myPlotter=MyPlotter::getPlotter(this);
				if (myPlotter) {
					extraD3XRot=myPlotter->mouseOffset.y()+myPlotter->tmpMouseOffset.y();
					extraD3ZRot=myPlotter->mouseOffset.x()+myPlotter->tmpMouseOffset.x();
				}
				const Mx4 tf=rotX(deg2rad(float(d3Xrot+extraD3XRot)))*rotZ(deg2rad(float(d3Zrot+extraD3ZRot)));

				project3d(p, x1, y1, tf);
				project3d(p, x2, y2, tf);
			}
		}
	}
}

void AspectRatioPixmapLabel::mousePressEvent(QMouseEvent *e) {
	const double x=double(mapFromGlobal(QCursor::pos()).x())/width();
	const double y=double(mapFromGlobal(QCursor::pos()).y())/height();
	QPointF pt=QPointF(x,y);

	const bool ctrlPressed=e->modifiers().testFlag(Qt::ControlModifier);
	const bool leftButtonPressed=e->buttons() & Qt::LeftButton;
	const bool rightButtonPressed=e->buttons() & Qt::RightButton;

	const bool rightButton=rightButtonPressed || (leftButtonPressed && ctrlPressed);

	if (rightButton) {
		ch1=pt;
		ch2=ptNull;
		ch2MoveStartOffset=ch2Offset=pt0;
	} else if (leftButtonPressed && ch2!=ptNull && QRectF(ch1,ch2).contains(pt)) {
		ch2MoveStartOffset=pt;
		ch2Offset=pt0;
	} else if (leftButtonPressed) {
		ch1=ptNull;
		ch2=ptNull;
		ch2MoveStartOffset=ch2Offset=pt0;
	}

	repaintAll();
}

void AspectRatioPixmapLabel::mouseMoveEvent(QMouseEvent *e) {
	const double x=double(mapFromGlobal(QCursor::pos()).x())/width();
	const double y=double(mapFromGlobal(QCursor::pos()).y())/height();
	QPointF pt=QPointF(x,y);

	const bool ctrlPressed=e->modifiers().testFlag(Qt::ControlModifier);
	const bool leftButtonPressed=e->buttons() & Qt::LeftButton;
	const bool rightButtonPressed=e->buttons() & Qt::RightButton;

	const bool rightButton=rightButtonPressed || (leftButtonPressed && ctrlPressed);

	if (rightButton) {
		ch2=pt;
		repaintAll();
	} else if (leftButtonPressed) {
		ch2Offset=pt-ch2MoveStartOffset;
		repaintAll();
	}
}

void AspectRatioPixmapLabel::mouseReleaseEvent(QMouseEvent *) {
	ch1+=ch2Offset;
	ch2+=ch2Offset;
	ch2MoveStartOffset=ch2Offset=pt0;
}



void AspectRatioPixmapLabel::repaintAll() const {
	for(auto *win:MainWindow::instances) {
        win->setCustomWindowTitle(label(), "");
		for(auto *img : win->images) { img->update(); }
	}
}

void AspectRatioPixmapLabel::save(const QString &fname) const {
#if LT_QT6
    pixmap()->save(fname);
#else
    pixmap().save(fname);
#endif
}

QString AspectRatioPixmapLabel::label() const {
	QString label;
	const int w(width());
	const int h(height());

	const int &x1=int((ch1.x()+ch2Offset.x())*w), &y1=int((ch1.y()+ch2Offset.y())*h);
	const int &x2=int((ch2.x()+ch2Offset.x())*w), &y2=int((ch2.y()+ch2Offset.y())*h);

	const bool p1=x1>=0 && y1>=0;
	const bool p2=x2>=0 && y2>=0;

	if (p1 && p2) { label=QString("%1,%2 -> %3,%4 [%5 x %6]").arg(x1).arg(y1).arg(x2).arg(y2).arg(x2-x1).arg(y2-y1);
	} else if (p1) { label=QString("%1,%2").arg(x1).arg(y1);
	}
	return label;
}
