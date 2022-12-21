#include "plot_myqcustomplotter.h"

#include "mainwindow.h"

const QPointF ptNull=QPointF(-1.123,-1.321);
const QPointF pt0=QPointF(0,0);
QPointF ch1; // crosshair
QPointF ch2; // rectangle "crosshair"
QPointF ch2MoveStartOffset; // if moving rectangle -- starting offset
QPointF ch2Offset; // offset to add

MyQCustomPlotter::MyQCustomPlotter() {
    setMouseTracking(true);
    title=new QCPTextElement(this, "TITLE", QFont("sans", 10));
    plotLayout()->insertRow(0);
    plotLayout()->addElement(0, 0, title);

	connect(this, &QCustomPlot::plottableClick, this, &MyQCustomPlotter::on_plottableClick);
	connect(this, &QCustomPlot::itemClick, this, &MyQCustomPlotter::on_itemClick);
	connect(this, &QCustomPlot::legendClick, this, &MyQCustomPlotter::on_legendClick);
	connect(this, &QCustomPlot::selectionChangedByUser, this, &MyQCustomPlotter::on_selectionChangedByUser);
}


void MyQCustomPlotter::paintEvent(QPaintEvent *e) {
    QCustomPlot::paintEvent(e);
    if (true) {
        QPainter p(this);

        const int w(width()); const int h(height());
        const int &x1=int((ch1.x()+ch2Offset.x())*w), &y1=int((ch1.y()+ch2Offset.y())*h);
        const int &x2=int((ch2.x()+ch2Offset.x())*w), &y2=int((ch2.y()+ch2Offset.y())*h);

        const bool p1=x1>0 && y1>0;
        const bool p2=x2>=0 && y2>=0;
		
		const QFont oldFont = p.font();
		QFont font=p.font();
		font.setPointSize(font.pointSize() * 1.5);
		p.setFont(font);

        if (p1) {
            p.setPen(p2 ? Qt::gray : Qt::black);
            p.setOpacity(1);

            p.drawLine(x1, 0, x1, h);
            p.drawLine(0, y1, w, y1);
            
            if (plottable()) {
                double xx1,yy1; plottable()->pixelsToCoords(x1,y1, xx1,yy1);
                p.setPen(Qt::black);
                p.drawText(x1+10,y1-10,QString("(%1, %2)").arg(xx1).arg(yy1));
            }
        }

        if (p2) {
            p.setPen(Qt::gray);
            p.setOpacity(1);

            p.drawLine(x2, 0, x2, h);
            p.drawLine(0, y2, w, y2);

            QColor fill=QColor(Qt::yellow).lighter();
            fill.setAlpha(80);
            p.fillRect(QRect(x1,y1, x2-x1,y2-y1), fill);
			
            if (plottable()) {
                double xx1,yy1; plottable()->pixelsToCoords(x1,y1, xx1,yy1);
                double xx2,yy2; plottable()->pixelsToCoords(x2,y2, xx2,yy2);
				
                p.setPen(Qt::black);
                p.drawText(x1+10,y1-10,QString("(%1, %2)").arg(xx1).arg(yy1));
                p.drawText(x2+10,y2-10,QString("(%1, %2)").arg(xx2).arg(yy2));
                p.drawText(std::min(x1,x2),(y1+y2)/2,QString("w:%1, h:%2)").arg(abs(xx1-xx2)).arg(abs(yy1-yy2)));
            }
        }

        if (plottable()) {
            p.setPen(Qt::black);
            for(const double x:verticals) {
                const QPointF px=plottable()->coordsToPixels(x,x);
                p.drawLine(px.x(),0, px.x(),height());
            }

            p.setPen(Qt::black);
            for(const double y:horizontals) {
                const QPointF px=plottable()->coordsToPixels(y,y);
                p.drawLine(0,px.y(), width(),px.y());
            }
        }
	}
}

void MyQCustomPlotter::mousePressEvent(QMouseEvent *e) {
    QCustomPlot::mousePressEvent(e);

    const double x=double(mapFromGlobal(QCursor::pos()).x())/width();
    const double y=double(mapFromGlobal(QCursor::pos()).y())/height();
    QPointF pt=QPointF(x,y);

	//    const bool ctrlPressed=e->modifiers().testFlag(Qt::ControlModifier);
    const bool leftButtonPressed=e->buttons() & Qt::LeftButton;
    const bool midButtonPressed=e->buttons() & Qt::MiddleButton;
    const bool rightButtonPressed=e->buttons() & Qt::RightButton;

    const bool rightButton=rightButtonPressed; // || (leftButtonPressed && ctrlPressed);

    if (rightButton) {
        ch1=pt;
        ch2=ptNull;
        ch2MoveStartOffset=ch2Offset=pt0;
    } else if (midButtonPressed && ch2!=ptNull && QRectF(ch1,ch2).contains(pt)) {
        ch2MoveStartOffset=pt;
        ch2Offset=pt0;
    } else if (leftButtonPressed) {
        ch1=ptNull;
        ch2=ptNull;
        ch2MoveStartOffset=ch2Offset=pt0;
    }

    repaintAll();
}

void MyQCustomPlotter::mouseMoveEvent(QMouseEvent *e) {
    QCustomPlot::mouseMoveEvent(e);

    const double x=double(mapFromGlobal(QCursor::pos()).x())/width();
    const double y=double(mapFromGlobal(QCursor::pos()).y())/height();
    QPointF pt=QPointF(x,y);

	//    const bool ctrlPressed=e->modifiers().testFlag(Qt::ControlModifier);
	//    const bool leftButtonPressed=e->buttons() & Qt::LeftButton;
    const bool midButtonPressed=e->buttons() & Qt::MiddleButton;
    const bool rightButtonPressed=e->buttons() & Qt::RightButton;
    const bool rightButton=rightButtonPressed; // || (leftButtonPressed && ctrlPressed);

    if (rightButton) {
        ch2=pt;
        repaintAll();
    } else if (midButtonPressed) {
        ch2Offset=pt-ch2MoveStartOffset;
        repaintAll();
    }
}

void MyQCustomPlotter::mouseReleaseEvent(QMouseEvent *e) {
    QCustomPlot::mouseReleaseEvent(e);

    ch1+=ch2Offset;
    ch2+=ch2Offset;
    ch2MoveStartOffset=ch2Offset=pt0;
}

void MyQCustomPlotter::repaintAll() {
    for(auto *win:MainWindow::instances) {
//        win->setCustomWindowTitle(label(), "");
        for(QCustomPlot *img : win->qcustomPlotImages) { img->replot(); }
    }
}

QString MyQCustomPlotter::label() const {
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


void MyQCustomPlotter::updateSelection(int sel) {
    for(int i=0; i<legend->itemCount(); i++) {
        plottable(i)->setSelection(QCPDataSelection(QCPDataRange()));
        legend->item(i)->setSelected(false);
    }
    //for(int i:selectedIdxs) {
	const int i=sel;
        if (i>=0 && i<plottableCount()) {
            plottable(i)->setSelection(QCPDataSelection(QCPDataRange(0,1e9)));
            legend->item(i)->setSelected(true);
        }
    //}
    replot();
}


#include <iostream>

void MyQCustomPlotter::on_plottableClick(QCPAbstractPlottable *plottable, int, QMouseEvent *) {
	for(int i=0; i<plottableCount(); i++) {
		if (this->plottable(i)==plottable) {
			updateSelection(i);
			MainWindow::instances[0]->setSelectedTrace(i, false); // TODO might not be the correct window

			break;
		}
	}
}


void MyQCustomPlotter::on_itemClick(QCPAbstractItem *i, QMouseEvent *e) {
    (void) i; (void) e;
}

void MyQCustomPlotter::on_legendClick(QCPLegend *l, QCPAbstractLegendItem *item, QMouseEvent *) {
	for(int i=0; i<l->itemCount(); i++) {
		if (l->item(i)==item) {
			updateSelection(i);
			MainWindow::instances[0]->setSelectedTrace(i, false); // TODO might not be the correct window
			break;
		}
	}
}

void MyQCustomPlotter::on_selectionChangedByUser() {
}

