// All(most) stuff related to the tree containing the plots and traces.

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <iostream>
#include <QDebug>
#include <QTimer>
#include <QScrollBar>
#include <QInputDialog>

#include "plot_myplotter.h"
#include "plot_myqcustomplotter.h"
#include "misc.h"

#define CUSTOM_ROLE Qt::UserRole

QList<QList<QVariant>> sel_to_pos(QTreeWidget *tree) {
	QList<QList<QVariant>> ret;
	auto sel=tree->selectedItems();
	for(auto x:sel) { ret<<x->data(0, CUSTOM_ROLE).toList(); }
	return ret;
}

QList<Plot *> sel_to_plot(QTreeWidget *tree, Set &set, bool include_children) {
	QList<Plot *> ret;
	for(auto *sel:tree->selectedItems()) {
		QList<QVariant> pos=sel[0].data(0, CUSTOM_ROLE).toList();
		auto *x=pos_to_plot(set, pos, include_children);
		if (x && !ret.contains(x)) { ret<<x; }
	}
	return ret;
}
QList<Trace *> sel_to_trace(QTreeWidget *tree, Set &set) {
	QList<Trace *> ret;
	for(auto *sel:tree->selectedItems()) {
		QList<QVariant> pos=sel[0].data(0, CUSTOM_ROLE).toList();
		auto *x=pos_to_trace(set, pos);
		if (x) { ret<<x; }
	}
	return ret;
}

Trace *item_to_trace(QTreeWidgetItem *item, Set &set) {
	QList<QVariant> pos=item->data(0, CUSTOM_ROLE).toList();
	return pos_to_trace(set, pos);
}
Plot *item_to_plot(QTreeWidgetItem *item, Set &set, bool include_children) {
	QList<QVariant> pos=item->data(0, CUSTOM_ROLE).toList();
	return pos_to_plot(set, pos, include_children);
}

QTreeWidgetItem *pos_to_item(QTreeWidget *tree, const QList<QVariant> &pos) {
	QTreeWidgetItem *item=tree->invisibleRootItem();
	for(auto p:pos) {
		if (!item) { break; }
		item=item->child(p.toInt());
	}
	return item;
}
QTreeWidgetItem *pos_to_item(QTreeWidget *tree, const QList<int> &pos) {
	QTreeWidgetItem *item=tree->invisibleRootItem();
	for(auto p:pos) {
		if (!item) { break; }
		item=item->child(p);
	}
	return item;
}
QVariant str_to_qvariant(const QString &txt) {
	bool ok;

	const int i=txt.toInt(&ok);
	if (ok) { return QVariant(i); }

	const double v=txt.toDouble(&ok);
	if (ok) { return QVariant(v); }

	return QVariant(txt);
}


void MainWindow::refresh_tree(bool keep_sel) {
	const auto old_sel=sel_to_pos(ui->tree);
	const auto old_scroll=ui->tree->verticalScrollBar()->value();

	int i=0;
	if (keep_sel) {
		BLOCKED_CALL(ui->tree, clear())
	} else {
		ui->tree->clear();
	}
	for(const Plot *plot:set.plots) {
		QTreeWidgetItem *treeItem=new QTreeWidgetItem(QStringList()<<plot->toString(EmptyOverride));
		treeItem->setData(0, CUSTOM_ROLE, QList<QVariant>()<<QVariant(i));
		treeItem->setCheckState(0, plot->expanded ? Qt::Checked : Qt::Unchecked);
		ui->tree->addTopLevelItem(treeItem);

		int j=0;
		for(Trace *trace:plot->traces) {
			QTreeWidgetItem *wg=new QTreeWidgetItem(QStringList()<<trace->toString(EmptyOverride));
			wg->setData(0, CUSTOM_ROLE, QList<QVariant>()<<QVariant(i)<<QVariant(j));
			wg->setCheckState(0, trace->enabled ? Qt::Checked : Qt::Unchecked);
			treeItem->addChild(wg);
			j++;
		}
		if (plot->expanded) { treeItem->setExpanded(true); }
		i++;
	}

	if (keep_sel) {
		ui->tree->blockSignals(true);
		for(auto sel:old_sel) {
			QTreeWidgetItem *wg=pos_to_item(ui->tree, sel);
			if (wg) { wg->setSelected(true); }
		}
		ui->tree->blockSignals(false);
		ui->tree->verticalScrollBar()->setValue(old_scroll);
	}
}


int index(QTreeWidgetItem *root, QTreeWidgetItem *x) {
    for(int i=0; i<root->childCount(); i++) {
        if (x==root->child(i)) { return i; }
    }
    return -1;
}

void MainWindow::setSelectedTrace(int traceIdx, bool blockSignals)
{
    GET_SEL_P1(true);
    if (plot) {
        const QList<Trace*> selectedTraces=sel_to_trace(ui->tree, set);
        const int selTraceIdx=(selectedTraces.size()==0 ? -1 : plot->traceIdx(selectedTraces[0],false));
        if (selTraceIdx!=traceIdx) {
            for(int i=0; i<plot->traces.size(); i++) {
                QTreeWidgetItem *trace=pos_to_item(ui->tree, QList<int>()<<set.plots.indexOf(plot)<<i);
                ui->tree->blockSignals(true);
                trace->setSelected(false);
                ui->tree->blockSignals(false);
            }

            ui->tree->blockSignals(blockSignals);
            QTreeWidgetItem *traceItem=pos_to_item(ui->tree, QList<int>()<<set.plots.indexOf(plot)<<plot->allIdxToEnabledIdx(traceIdx));
			if (traceItem) {
				// Might be nullptr when we selecte an entry that's not a trace, e.g. the gap for a StackedLine.
	            traceItem->setSelected(true);
			}
            ui->tree->blockSignals(false);
        }
    }
}

void MainWindow::on_tree_itemChanged(QTreeWidgetItem *item, int column) {
	const bool checked=item->checkState(column)==Qt::Checked;
	Trace *trace=item_to_trace(item, set);
	Plot *plot=item_to_plot(item, set, false);

	push_undo("Toggle","plot/trace state");

	// We use a QTimer here, because if we don't use it, the program crashes.
	// I haven't figured out why exactly, but there is an invalid read in ui->tree->clear(),
	// which is created inside refresh_tree(). So, my guess is that there is some reference
	// to a QTreeWidgetItem (maybe the one in this function?) that is deleted.
	// The singleShot ensures that all references have been processed, I think.
	if (plot && plot->expanded!=checked) { plot->expanded=checked; QTimer::singleShot(0,[this]{ on_update(true,RefreshTreeKeepSel); });
	} else if (trace && trace->enabled!=checked) { trace->enabled=checked; on_update(true,NoRefresh);
	}
}

void MainWindow::on_tree_itemSelectionChanged() {
	GET_SEL_PT1(true);
	GET_SEL_PT(false);


	QList<QWidget *> block_signals_widgets=QList<QWidget *>()
		<<ui->tabs
		<<ui->txtPlotDefaultQuery<<ui->txtPlotTitle
		<<ui->chkLogX<<ui->chkLogY<<ui->chkLogZ
		<<ui->chkXTic<<ui->chkYTic<<ui->chkZTic
		<<ui->txtXAxisLabel<<ui->txtYAxisLabel<<ui->txtZAxisLabel
		<<ui->txtXMin<<ui->txtXMax
		<<ui->txtYMin<<ui->txtYMax
		<<ui->txtZMin<<ui->txtZMax
		<<ui->chkD3Grid<<ui->txtXrot<<ui->txtZrot
		<<ui->txtTraceTitle<<ui->txtTraceQuery<<ui->cmbTraceType<<ui->cmbTraceX<<ui->cmbTraceY<<ui->cmbTraceZ
		<<ui->lblTraceStyleOptions<<ui->txtFinalQuery;

	QList<QWidget *> if_no_trace_selected=QList<QWidget *>()
		<<ui->btnQueryData<<ui->tblTraceVars
		<<ui->txtFinalQuery<<ui->btnAddTraceVar<<ui->btnDeleteTraceVar;

	for(QWidget *wg : block_signals_widgets) { wg->blockSignals(true); }
	for(auto *wg:if_no_trace_selected) { wg->setEnabled(trace); }

	ui->btnCheck->setEnabled(plots.size()>0);

	if (true) {
		const QStringList varNames=merge(set.globalVars,
			 merge( plot ? plot->variables : MapQStringQString(),
				trace ? trace->variables : MapQStringQString() )).keys();
        hlPlotDefaultQuery->setVariableNames(varNames);
        hlTraceQuery->setVariableNames(varNames);
        hlFinalQuery->setVariableNames(varNames);
	}

	// Count the number of rows, and show it in the tab text
    if (trace) {
		QStringList col_names=trace->column_names(DoUseTraceQuery, NoUngroup, EmptyOverride);
		col_names.insert(0,"");

		QList<QComboBox*> cmbs= QList<QComboBox*>()<<ui->cmbTraceX<<ui->cmbTraceY<<ui->cmbTraceZ;
		for(QComboBox *cmb :cmbs) {
			cmb->clear();
			cmb->addItems(col_names);
		}

		ui->txtTraceTitle->setText(trace->title);
		ui->txtTraceQuery->setPlainText(trace->query);

		ui->cmbTraceX->setCurrentIndex(ui->cmbTraceX->findText(trace->x.toUpper()));
		ui->cmbTraceY->setCurrentIndex(ui->cmbTraceY->findText(trace->y.toUpper()));
		ui->cmbTraceZ->setCurrentIndex(ui->cmbTraceZ->findText(trace->z.toUpper()));
		refresh_cmbTraceType();
		ui->cmbTraceType->setCurrentIndex(ui->cmbTraceType->findText(trace->style ? trace->style->name() : ""));
		ui->lblTraceStyleOptions->setText(trace->style ? trace->style->toString() : "");

		ui->chkXTic->setChecked(trace->xtic);
		ui->chkYTic->setChecked(trace->ytic);
		ui->chkZTic->setChecked(trace->ztic);

		// Also modified in MainWindow::on_update()!
		ui->txtFinalQuery->setPlainText(QString(trace ? trace->get_query(EmptyOverride) : "").replace(QRegularExpression("[\\t \\n]+"), " "));
		ui->tabs->setTabText(3, QString("Data (%1)").arg(trace->rowCount(DoUseTraceQuery, ui->chkUngroup->isChecked() ? Ungroup : NoUngroup, EmptyOverride)));
		refresh_lstTraceVars();

		if (!restoringState) {
			if (set.plotMode==Set::PlotMode::MyPlotter && images.size()>0) {
				AspectRatioPixmapLabel *lbl=images.back();
				Q_ASSERT(lbl!=nullptr);
				if (lbl!=nullptr && GeneratePlotTask::myPlotters.contains(lbl) && plot) {
					MyPlotter *myPlotter=GeneratePlotTask::myPlotters[lbl];
					myPlotter->states[lbl].selSeries=plot->traceIdx(trace, false);
					myPlotter->repaint(lbl);
				}
			}
			if (set.plotMode==Set::PlotMode::QCustomPlot && qcustomPlotImages.size()>0) {
				MyQCustomPlotter *lbl=qcustomPlotImages.back();
				Q_ASSERT(lbl!=nullptr);
				const int idx=plot->traceIdx(trace,false);
				lbl->updateSelection(idx);
			}
		}
	} else { ui->tabs->setTabText(3, QString("Data"));
	}

	if (plot) {
		plot->apply(*this);
		refresh_lstPlotVars();
	}

	ui->tblData->setRowCount(0);

	// TODO depending on dimension, set available plot types

	for(QWidget *wg : block_signals_widgets) { wg->blockSignals(false); }

	if (set.autoReplot && (plot!=sel_plot || plots.size()>1)) { on_btnPlot_clicked(); }
	if (ui->tabs->currentWidget()==ui->tabData) { on_btnQueryData_clicked(); }


	ui->btnClone->setEnabled(ui->tree->selectedItems().size()==1);
	ui->btnMulticlone->setEnabled(ui->tree->selectedItems().size()==1);
//	ui->btnMulticlone2->setEnabled(ui->tree->selectedItems().size()==1);
	ui->btnMoveUp->setEnabled(ui->tree->selectedItems().size()==1);
	ui->btnMoveDown->setEnabled(ui->tree->selectedItems().size()==1);

	ui->btnExportMenu->setText(QString("Export selection (%1)").arg(plots.size()));
	ui->btnExportMenu->setEnabled(plots.size()>0);

	sel_plot=plot;
}

void MainWindow::on_btnNewPlot_clicked() {
	push_undo("Add","new plot");
	set.plots<<new Plot(set);
	set.plots.back()->traces<<new Trace(*set.plots.back());
	on_update();
}

void MainWindow::on_btnNewTrace_clicked() {
	GET_SEL_P1(true);
	if (!plot) { return; }

	push_undo("Add new","trace");
	plot->traces<<new Trace(*plot);
	on_update();
}

void MainWindow::on_btnClone_clicked() {
	GET_SEL_PT1(true);

    if (trace) {  push_undo("Clone","trace"); plot->traces<<trace->clone();
	} else if (plot) {  push_undo("Clone","plot"); set.plots<<plot->clone();
	}
	on_update();
}


void MainWindow::on_btnMulticlone_clicked() {
	GET_SEL_PT1(false);
	if (!trace && !plot) { return; }

	const QString descr=QString("Enter something like VARa=VALUE1a VALUE2a VALUE3a.\nVARb=VALUE1b VALUE2b\n\n"
				    "It will clone the current entry, and replace VAR with each value. "
				    "VALUEs are seperated by space. \n"
				    "Variables that are defined for the current trace: %1");

	const QStringList vars=(trace ? trace->variables : plot->variables).keys();
    const QString table=(trace ? trace->getTableName() : plot->getTableName()); // TODO maybe use query with variables expanded?
	QString default_input;
	for(auto &var:vars) {
        default_input.append(QString("%1 = %2\n").arg(var).arg(db.distinctValues(var, table).join(" ")));
	}
	const QString x=QInputDialog::getMultiLineText(this, "Enter variable and its values"
						       , descr.arg(vars.join(", ")), default_input);
	const QMap<QString,QStringList> cp_vars=lines_to_map(x.split("\n"));
	const unsigned n=n_combinations(cp_vars);
	if (cp_vars.size()==0 || n==0) { return; }


    if (trace) {
		push_undo("Multiclone","trace");

		Plot *plot=trace->plot;
		for(unsigned i=0; i<n; i++) {
			plot->traces<<trace->clone();
			MapQStringQString &vars=plot->traces.back()->variables;

			const auto cp_var=nth_combination(cp_vars, i);
			for(QString k:cp_var.keys()) { vars[k]=cp_var[k]; }

			// check if the last created trace is a copy of a previous trace.
			for(int j=0; j<plot->traces.size()-1; j++) {
				if (plot->traces[j]->variables==vars) {
					delete plot->traces.back(); // warning: vars is not valid anymore.
					plot->traces.pop_back();
					break;
				}
			}
		}
	} else if (plot) {
		push_undo("Multiclone","plot");

		for(unsigned i=0; i<n; i++) {
			set.plots<<plot->clone();
			MapQStringQString &vars=set.plots.back()->variables;

			const auto cp_var=nth_combination(cp_vars, i);
			for(QString k:cp_var.keys()) { vars[k]=cp_var[k]; }
		}
	}

	on_update();
}

template<class F>
void clone2_helper(const QStringList &todo_vars, const QMap<QString,QStringList> &uniq_values, QMap<QString,QString> &ready_values, F do_it) {
	if (todo_vars.size()==0) { do_it(ready_values); return; }
	const QString var=*todo_vars.begin();
	QStringList leftover_vars=QStringList(todo_vars); leftover_vars.pop_front();
	for(const QString &uniq_value : uniq_values[var]) {
		ready_values[var]=uniq_value;
		clone2_helper(leftover_vars, uniq_values, ready_values, do_it);
	}
}

void MainWindow::on_btnMulticlone2_clicked() {
	GET_SEL_T1;
	if (!trace) { return; }

	QStringList valid_vars, suitable_vars;
	const QStringList vars=trace->variables.keys();

	QMap<QString,QStringList> uniq_values;
	const Trace::Rows *data=trace->data(QStringList(), DoNotUseTraceQuery, NoUngroup, EmptyOverride);
	if (!data) { return; }

	int idx=0;
	for(const QString &col_name:trace->column_names(DoNotUseTraceQuery, NoUngroup, EmptyOverride)) {
		if (vars.contains(col_name)) {
			if (trace->variables[col_name].length()==0) { suitable_vars<<col_name; }

			QStringList values;
			for(const QList<QVariant> &row : *data) {
				if (!values.contains(row[idx].toString())) { values<<row[idx].toString(); }
			}
			uniq_values[col_name]=values;
			valid_vars<<QString("%1 (%2)").arg(col_name).arg(values.join(","));
		}
		idx++;
	}

	QString x=QInputDialog::getMultiLineText(this, "Enter variables"
						 , QString("Enter a space-separated list of variables, like VAR1 VAR2 ... VARN.\n"
							   "It will create a new trace for every unique value of the combination of VAR1xVAR2x..xVARN.\n\n"
							   "Following variables are also column names, and thus can be used: \n\n%1").arg(valid_vars.join("\n"))
						 , suitable_vars.join(" "));
	if (x.length()==0) { return; }

	push_undo("Multiclone2","trace");

	Plot *plot=trace->plot;
	const QStringList todo_vars=x.split(QRegularExpression("\\s+"));
	QMap<QString,QString> ready_values;
	clone2_helper(todo_vars, uniq_values, ready_values,
		      [plot,trace](QMap<QString,QString> &values) {
		DEBUG("Cloning with "<<values);
		Trace *clone=trace->clone();
		plot->traces<<clone;

		for(const QString &var:values.keys()) { clone->variables[var]=values[var]; }
	});

	on_update();
}



void MainWindow::on_btnDelete_clicked() {
	GET_SEL_PT(false);

	push_undo("Delete","plot/trace");
	for(Trace *trace:traces) {
		trace->plot->traces.removeAll(trace);
		delete trace;
	}
	for(Plot *plot:plots) {
		set.plots.removeAll(plot);
		delete plot;
	}

	on_update();
}

void MainWindow::on_btnMoveUp_clicked() {
	GET_SEL_PT1(false);

    if (trace) {
		push_undo("Move","trace up");
		const int i=trace->plot->traces.indexOf(trace);
		if (i>0) { trace->plot->traces.move(i,i-1); }
	} else if (plot) {
		push_undo("Move","plot up");
		const int i=plot->set->plots.indexOf(plot);
		if (i>0) { plot->set->plots.move(i,i-1); }
	} else {
		return;
	}

	refresh_tree(false);
	QTreeWidgetItem *item=pos_to_item(ui->tree, trace ? trace->pos() : plot->pos());
	if (item) { item->setSelected(true); }
	if (set.autoReplot) { on_btnPlot_clicked(); }
}

void MainWindow::on_btnMoveDown_clicked() {
	GET_SEL_PT1(false);

    if (trace) {
		push_undo("Move","trace down");
		const int i=trace->plot->traces.indexOf(trace);
		if (i<trace->plot->traces.size()-1) { trace->plot->traces.move(i,i+1); }
	} else if (plot) {
		push_undo("Move","plot up");
		const int i=plot->set->plots.indexOf(plot);
		if (i<plot->set->plots.size()-1) { plot->set->plots.move(i,i+1); }
	} else {
		return;
	}

	refresh_tree(false);
	QTreeWidgetItem *item=pos_to_item(ui->tree, trace ? trace->pos() : plot->pos());
	if (item) { item->setSelected(true); }
	if (set.autoReplot) { on_btnPlot_clicked(); }
}

void MainWindow::on_btnSortTraces_clicked() {
	GET_SEL_P1(true);
	if (!plot) { return; }
	push_undo("Sort traces","by title");
	std::sort(plot->traces.begin(), plot->traces.end(), [](const Trace *l_, const Trace *r_) {
		const QString l=l_->get_title(l_->variables);
		const QString r=r_->get_title(r_->variables);
		return l<r;
	});

	refresh_tree(false);
	QTreeWidgetItem *item=pos_to_item(ui->tree, plot->pos());
	if (item) { item->setSelected(true); }
	if (set.autoReplot) { on_btnPlot_clicked(); }
}


void MainWindow::on_btnCheckAll_clicked() {
	GET_SEL_P(true);
	if (plots.size()==0) { return; }
	push_undo("Check","all");
	for(auto *plot:plots) {
		for(Trace *t:plot->traces) { t->enabled=true; }
	}

	refresh_tree(true);
	if (set.autoReplot) { on_btnPlot_clicked(); }
}

void MainWindow::on_btnUncheckAll_clicked() {
	GET_SEL_P(true);
	if (plots.size()==0) { return; }
	push_undo("Uncheck","all");
	for(auto *plot:plots) {
		for(Trace *t:plot->traces) { t->enabled=false; }
	}

	refresh_tree(true);
	if (set.autoReplot) { on_btnPlot_clicked(); }
}

void MainWindow::on_btnCheckWithData_clicked() {
	GET_SEL_P(true);
	if (plots.size()==0) { return; }
	push_undo("Check","w/ data");
	for(auto *plot:plots) {
		for(Trace *t:plot->traces) {
			t->enabled = t->rowCount(DoUseTraceQuery, ui->chkUngroup->isChecked() ? Ungroup : NoUngroup, EmptyOverride)>0;
		}
	}

	refresh_tree(true);
	if (set.autoReplot) { on_btnPlot_clicked(); }
}
