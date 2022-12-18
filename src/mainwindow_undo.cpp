// Stuff related to undo and redo

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include <QScrollBar>
#include <QInputDialog>

#include "plot_myplotter.h"
#include "misc.h"

void MainWindow::clear_history() {
	for(HistEntry &e : undo_history) { delete e.second; }
	undo_history.clear();
	updateHistoryButtons();
}

void MainWindow::push_undo(const QString &p1, const QString &p2) {
	if (restoringState) { return; }
	undo_history<<QPair<QString,Set *>(QString("%1\n%2").arg(p1,p2),set.clone());
	for(HistEntry &e : redo_history) { delete e.second; }
	redo_history.clear();
	while (undo_history.size()>20) { undo_history.pop_front(); }
	updateHistoryButtons();
}

void MainWindow::undo() {
	if (undo_history.size()==0) { return; }

	redo_history.insert(0,HistEntry(undo_history.back().first,set.clone()));
	set=*undo_history.back().second;
	delete undo_history.back().second;
	undo_history.pop_back();
	post_xdo(true);
}

void MainWindow::redo() {
	if (redo_history.size()==0) { return; }

	undo_history.push_back(HistEntry(redo_history.front().first,set.clone()));
	set=*redo_history.front().second;
	delete redo_history.front().second;
	redo_history.pop_front();
	post_xdo(true);
}

void MainWindow::post_xdo(const bool keep_sel) {
	refresh_tree(keep_sel);
	refresh_lstGlobalVars();
	refresh_lstPlotVars();
	refresh_lstTraceVars();

	BLOCKED_CALL(ui->chkAutoReplot, setChecked(set.autoReplot))
	BLOCKED_CALL(ui->cmbPlotMode, setCurrentIndex(set.plotMode))
	BLOCKED_CALL(ui->chkStableColors, setChecked(set.stableColors))
	BLOCKED_CALL(ui->chkBBox, setChecked(set.bbox))
	updateHistoryButtons();
	on_btnPlot_clicked(); // it is best to always do this, regardless of chkAutoReplot. If not, we might crash. I think due to an invalid plot reference in MyPlotter.
	if (keep_sel) {  on_tree_itemSelectionChanged(); }
}

void MainWindow::updateHistoryButtons() {
//    ui->btnUndo->setText(QString("U %1 (%2)").arg(undo_history.size() ? undo_history.back().first.mid(0,30) : "\n").arg(undo_history.size()));
//	ui->btnRedo->setText(QString("R %1 (%2)").arg(redo_history.size() ? redo_history.front().first.mid(0,30) : "\n").arg(redo_history.size()));
    ui->btnUndo->setText(QString("U %1 (%2)").arg("").arg(undo_history.size()));
    ui->btnRedo->setText(QString("R %1 (%2)").arg("").arg(redo_history.size()));
    ui->btnUndo->repaint();
	ui->btnRedo->repaint();
}
