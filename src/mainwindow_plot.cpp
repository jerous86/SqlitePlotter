#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <math.h>

#include "misc.h"
#include "plot_myplotter.h"
#include "plot_myqcustomplotter.h"

template<class T>
T *MainWindow::getWidgetForPlotMode(const int index) {
	switch (set.plotMode) {
	case Set::PlotMode::MyPlotter: return images[index];
	case Set::PlotMode::QCustomPlot: return qcustomPlotImages[index];
	default:
		qDebug()<<"Invalid mode "<<index;
		return nullptr;
	}
}


void MainWindow::on_btnPlot_clicked() {
	if (initializing) { return; }
	GET_SEL_P(true);

	ui->btnPlot->setFocus();

	for(int i=0; i<images.size(); i++) {
        ui->gridImages->removeWidget(images[i]);
        ui->gridImages->removeWidget(qcustomPlotImages[i]);
        images[i]->hide();
        qcustomPlotImages[i]->hide();
	}

	while (images.size()<storeFirstIndex+std::max(1,int(plots.size()))) {
		images<<new AspectRatioPixmapLabel(ui->chkKeepAR->isChecked());
        qcustomPlotImages<<new MyQCustomPlotter();
	}

	// At init. To avoid some "flicker" later on.
	if (plots.size()==0) {
		ui->gridImages->addWidget(getWidgetForPlotMode(0));
	}

	const int N=int(storeFirstIndex+plots.size());
	int n_cols=-1;
	if (N==1) { n_cols=1;
	} else if (N==2 || N==3 || N==4) { n_cols=2;
	} else if (N<=9) { n_cols=3;
	} else { n_cols=4; }

	if (N>1) { DEBUG("N "<<N<<", n_cols "<<n_cols); }

	for(int i=0; i<N; i++) {
		const int x=int(std::floor(double(i)/double(n_cols)));
		const int y=i%n_cols;
		auto *wg=getWidgetForPlotMode<>(i);
		ui->gridImages->addWidget(wg, x, y);
	}
	for(int i=0; i<N; i++) {
		getWidgetForPlotMode(i)->show();
		getWidgetForPlotMode(i)->updateGeometry();
	}

	for(int i=0; i<N; i++) {
		const int i2=i-storeFirstIndex;

		if (i<storeFirstIndex) { // store images
		} else { // these have to be replot
			Q_ASSERT(i2>=0);
			Plot *plot=plots[i2];
			GeneratePlotTask task(this, int(set.plots.indexOf(plot)));

			// WARNING: keep this processEvents disabled -- it does not cooperate well with MyPlotter.
			// QCoreApplication::processEvents();

			switch (set.plotMode) {
                case Set::PlotMode::MyPlotter: {
                    AspectRatioPixmapLabel *img=images[i];
                    img->setCustomPixmap(QPixmap());
                    MyPlotter::invalidateCache(img);
                    task.run_myPlotter(*plot, plotOptions, img, EmptyOverride);
                    break;
                }
                case Set::PlotMode::QCustomPlot: {
                    task.run_QCustomPlot(*plot, plotOptions, qcustomPlotImages[i], EmptyOverride);
                    break;
                }
            }
		}
	}
}

void MainWindow::on_btnZoomToRect_clicked()
{
    switch (set.plotMode) {
    case Set::PlotMode::QCustomPlot: {
        if (qcustomPlotImages.size()==0) { return; }
        if (ch1!=ptNull && ch2!=ptNull) {
            MyQCustomPlotter *img=qcustomPlotImages.back();
            if (img->plottableCount()>0) {
                auto *g=img->plottable(0);
				const int w=img->width(), h=img->height();
				double c1x,c1y,c2x,c2y;
				g->pixelsToCoords(ch1.x()*w, ch1.y()*h, c1x,c1y);
				g->pixelsToCoords(ch2.x()*w, ch2.y()*h, c2x,c2y);

				img->xAxis->setRange(c1x, c2x);
				img->yAxis->setRange(c1y, c2y);

				MyQCustomPlotter::repaintAll();
			}
        }
        break;
    default: break;
        }
    }
}

void MainWindow::on_btnStoreAdd_clicked() {
	GET_SEL_PT(true);
	storeFirstIndex+=plots.size();
	DEBUG("New storeFirstIndex "<<storeFirstIndex);
	on_btnPlot_clicked();
}

void MainWindow::on_btnStoreRemove_clicked() {
	if (storeFirstIndex>0) {
		storeFirstIndex-=1;
		DEBUG("New storeFirstIndex "<<storeFirstIndex);
		on_btnPlot_clicked();
	}
}

void MainWindow::on_btnClearStore_clicked() {
	storeFirstIndex=0;
	on_btnPlot_clicked();
}

void MainWindow::on_chkAutoReplot_toggled() {
	//	push_undo();
	set.autoReplot=ui->chkAutoReplot->isChecked();
	on_update(true, NoRefresh, false);
}

void MainWindow::on_chkLegend_toggled() {
    plotOptions.plotLegend=ui->chkLegend->isChecked();
	on_update(true, NoRefresh, true);
}

void MainWindow::on_chkTitle_toggled() {
    plotOptions.plotTitle=ui->chkTitle->isChecked();
	on_update(true, NoRefresh, true);
}
