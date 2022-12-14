#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QInputDialog>


#define COL_NAME 0
#define COL_VALUE 1

QStringList joinValues(const QStringList &values, const QString &scope, const QString &var, bool toUrl, const int maxStringLength, const int maxEntries) {
    QStringList result;
    int strLen=0;
    for(const QString &x : values) {
        result<<(toUrl ? QString("<a href='%1 §§ %2 §§ %3'>%3</a>").arg(scope,var,x) : x);
        strLen+=x.length();
        if (
            (maxStringLength>0 && strLen>maxStringLength) ||
            (maxEntries>0 && result.length()>maxEntries)
        ) { result<<"(...)"; break; }
    }
    return result;
}

QStringList oldValues(const MainWindow *main, const QString &scope, const QString &var, bool toUrl, const int maxStringLength, const int maxEntries) {
    return joinValues(main->used_values[var], scope, var, toUrl, maxStringLength, maxEntries);
}

QStringList uniqueValues(const MainWindow *main, const QString &table, const QString &scope, const QString &var, bool toUrl, const int maxStringLength, const int maxEntries) {
    return main->unique_values.contains(table+"_"+var)
            ? joinValues(main->unique_values[table+"_"+var], scope, var, toUrl, maxStringLength, maxEntries)
            : QStringList();
}

void updateVarPossibilities(MainWindow *main, const QString &table, const QString &scope, const QString &var) {
	QStringList xs;
    xs.append(uniqueValues(main, table, scope, var, true, 150, 0));
	xs<<"<br />[Old values] ";
	xs.append(oldValues(main, scope, var, true, 150, 0));

	if (xs.size()>1/*[Old values]*/) {
		QString lbl=QString("Unique(%2 <b>%1</b>) (<a href='hide §§ unique'>hide</a>):").arg(var).arg(scope);
		lbl+=xs.join(", ");
		lbl+=QString(" [<a href='%1 §§ %2 §§ __CMD__ §§ clear'>Clear</a>]").arg(scope).arg(var);
		// We do it in this convulated because when we have something like e.g. "%2g2u" "%2" is considered
		// a parameter. Escaping it does not seem to work :(
		main->ui->lblUniqueValues->setText(lbl);
	} else {
		main->ui->lblUniqueValues->setText("");
	}
}



void refresh_vars(const MapQStringQString &vars, QTableWidget *lst) {
	lst->setRowCount(0);

	lst->blockSignals(true);
	lst->setRowCount(int(vars.size()));
	int row=0;
    for(const QString &name : vars.keys()) {
		lst->setItem(row, COL_NAME, new QTableWidgetItem(name));
		lst->setItem(row, COL_VALUE, new QTableWidgetItem(vars[name]));
		lst->setRowHeight(row, 20);
		row++;
	}
    lst->blockSignals(false);
}

void MainWindow::refresh_lstGlobalVars() {
    refresh_vars(set.globalVars, ui->tblGlobalVars);
}

void MainWindow::refresh_lstPlotVars() {
	ui->tblPlotVars->setRowCount(0);
    GET_SEL_P1(true);
    if (plot) { refresh_vars(plot->variables, ui->tblPlotVars); }
}

void MainWindow::refresh_lstTraceVars() {
	ui->tblTraceVars->setRowCount(0);
	GET_SEL_T1;
	if (trace) { refresh_vars(trace->variables, ui->tblTraceVars); }
}

void on_var_selected(MainWindow *main, QTableWidget *lst, const QString &scope, const MapQStringQString &vars, const QString &table);

bool add_var(MainWindow *main, QTableWidget *lst, const QString &scope, MapQStringQString &vars, const QString &table) {
	const QString &name=QInputDialog::getText(nullptr, "Enter variable name", "Variable name").trimmed();
	if (name.length()==0) { return false; }

	QString var_name, value;
	if (name.contains("=")) {
		const QStringList pts=name.split("=");
		var_name=pts[0]; value=pts[1];
	} else {
		var_name=name;
		value=QInputDialog::getText(nullptr, "Enter value for "+name, "Value");
	}

	main->push_undo(scope, QString("+ %2").arg(scope,name));
	vars[var_name]=value;
	main->add_used_value(var_name, value);
    on_var_selected(main, lst, scope, vars, table);
	return true;
}

bool delete_var(MainWindow *main, const QString &scope, QTableWidget *lst, MapQStringQString &vars, const QString &table) {
	if (lst->selectedItems().size()==0) { return false; }

	const QString &name=lst->item(lst->selectedItems()[0]->row(), COL_NAME)->text();
	main->push_undo(scope, QString("⌫ %2").arg(scope,name));
	vars.remove(name);
    on_var_selected(main, lst, scope, vars, table);
	return true;
}

bool var_table_itemChanged(MainWindow *main, const QString &scope, QTableWidget *lst, QTableWidgetItem *item, MapQStringQString &vars, QString &old_col_name, const QString &table) {
	if (item->column()==COL_NAME) {
        const QString new_col_name=item->text();
		main->push_undo(scope, QString("%1->%2").arg(old_col_name,new_col_name));

		const QString value=vars[old_col_name];
		vars.remove(old_col_name);
		vars[new_col_name]=value;
		if (main->used_values.contains(old_col_name)) {
			// Merge ...
			for(const QString &value:main->used_values[old_col_name]) { main->add_used_value(new_col_name, value); }
			main->used_values.remove(old_col_name);
		}

	} else if (item->column()==COL_VALUE) {
		const QString name=lst->item(item->row(),COL_NAME)->text();
        const QString value=item->text();
		main->push_undo(scope, QString("%1->%2").arg(name,value));

		vars[name]=value;
		main->add_used_value(name, value);
	}
	old_col_name=item->text();
    on_var_selected(main, lst, scope, vars, table);
	return true;
}


void on_var_selected(MainWindow *main, QTableWidget *lst, const QString &scope, const MapQStringQString &vars, const QString &table) {
	if (lst->currentRow()<0) { return; }

	const QString var=lst->item(lst->currentRow(), COL_NAME)->text();

	if (vars.contains(var)) {
        if (!main->unique_values.contains(table+"_"+var)) {
            Db::Stmt q=main->db.query(QString("SELECT DISTINCT %1 FROM %2 ORDER BY %1 ASC").arg(var).arg(table), true);
			QStringList values;

			while (true) {
				auto row=q.next_row();
				if (row.size()==0) { break; }
				values<<row[0].toString();
			}
            if (values.size()) { main->unique_values[table+"_"+var]=values; }
		}
	}

	// Add current value to used values
	main->add_used_value(var, vars[var]);
    updateVarPossibilities(main, table, scope, var);
}

void var_currentItemChanged(MainWindow *main, QTableWidget *lst, QTableWidgetItem *current, QString &old_col_name, const QString &scope,const MapQStringQString &vars, const QString &table) { // selection changed
	old_col_name=current ? lst->item(current->row(),COL_NAME)->text() : "";
    on_var_selected(main, lst, scope, vars, table);
}

void MainWindow::on_lblUniqueValues_linkActivated(const QString &link) {
	const QStringList pts=link.split(" §§ ");
	if (pts[0]=="hide" && pts[1]=="unique") {
		ui->lblUniqueValues->setText("(hidden -- click a variable to show again)");
		return;
	}
	QString scope=pts[0].trimmed(), var=pts[1].trimmed(), value=pts[2];

	MapQStringQString *vars=nullptr;
	if (value=="__CMD__") {
		if (pts[3]=="clear") { used_values.remove(var); }
	} else if (scope=="global") {
		vars=&set.globalVars;
	} else if (scope=="plot") {
		GET_SEL_P1(true);
		// If we have no selection anymore, clear var possibilities by setting
		// to invalid.
		if (plot) { vars=&plot->variables; } else { var=""; }
	} else if (scope=="trace") {
		GET_SEL_T1;
		if (trace) { vars=&trace->variables; } else { var=""; }
	} else {
		qDebug()<<scope<<var<<value; Q_ASSERT(false);
	}

	if (vars) {
		push_undo(scope, QString("%1->%2").arg(var,value));
		(*vars)[var]=value;
		add_used_value(var, value);
	}

    QString table=DEFAULT_TABLE_NAME;
    if (scope=="global") {
        refresh_lstGlobalVars();
        table=DEFAULT_TABLE_NAME;
    } else if (scope=="plot") {
        refresh_lstPlotVars();
        GET_SEL_P1(true);
        if (plot) { table=plot->getTableName(); }
    } else if (scope=="trace") {
        refresh_lstTraceVars();
        GET_SEL_T1;
        if (trace) { table=trace->getTableName(); }
    }
    on_update();
    updateVarPossibilities(this, table, scope, var);
}


void MainWindow::on_btnAddPlotVar_clicked() {
	GET_SEL_P1(true);
	if (!plot) { return; }

    if (add_var(this, ui->tblPlotVars, "plot", plot->variables, plot->getTableName())) { refresh_lstGlobalVars(); on_update(); }
}

void MainWindow::on_btnDeletePlotVar_clicked() {
	GET_SEL_P1(true);
	if (!plot) { return; }

    if (delete_var(this, "plot", ui->tblPlotVars, plot->variables, plot->getTableName())) {	refresh_lstGlobalVars(); on_update(); }
}
void MainWindow::on_tblPlotVars_itemChanged(QTableWidgetItem *item) {
	if (!item) { return; }
	GET_SEL_P1(true);
	if (!plot) { return; }

    if (var_table_itemChanged(this, "plot", ui->tblPlotVars, item, plot->variables, tblPlotVars_old_col_name, plot->getTableName())) { on_update();  }
}

void MainWindow::on_tblPlotVars_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *) {
	GET_SEL_P1(true);
	if (!plot) { return; }

    var_currentItemChanged(this, ui->tblPlotVars, current, tblPlotVars_old_col_name, "plot", plot->variables, plot->getTableName());
}

void MainWindow::on_btnAddGlobalVar_clicked() {
    if (add_var(this, ui->tblGlobalVars, "global", set.globalVars, DEFAULT_TABLE_NAME)) { refresh_lstGlobalVars(); on_update(); }
}
void MainWindow::on_btnDeleteGlobalVar_clicked() {
    if (delete_var(this, "global", ui->tblGlobalVars, set.globalVars, DEFAULT_TABLE_NAME)) { refresh_lstGlobalVars(); on_update(); }
}
void MainWindow::on_tblGlobalVars_itemChanged(QTableWidgetItem *item) {
	if (!item) { return; }
    if (var_table_itemChanged(this, "global", ui->tblGlobalVars, item, set.globalVars, tblGlobalVars_old_col_name, DEFAULT_TABLE_NAME)) { on_update(); }
}

void MainWindow::on_tblGlobalVars_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *) {
    var_currentItemChanged(this, ui->tblGlobalVars, current, tblGlobalVars_old_col_name, "global", set.globalVars, DEFAULT_TABLE_NAME);
}

void MainWindow::on_btnAddTraceVar_clicked() {
	GET_SEL_T1;
    if (trace && add_var(this, ui->tblTraceVars, "trace", trace->variables, trace->getTableName())) { refresh_lstTraceVars(); on_update(); }
}
void MainWindow::on_btnDeleteTraceVar_clicked() {
	GET_SEL_T1;
    if (trace && delete_var(this, "trace", ui->tblTraceVars, trace->variables, trace->getTableName())) { refresh_lstTraceVars(); on_update(); }
}
void MainWindow::on_tblTraceVars_itemChanged(QTableWidgetItem *item) {
	if (!item) { return; }
	GET_SEL_T1;
    if (trace && var_table_itemChanged(this, "trace", ui->tblTraceVars, item, trace->variables, tblTraceVars_old_col_name, trace->getTableName())) { on_update(); }
}

void MainWindow::on_tblTraceVars_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *) {
	GET_SEL_T1;
    if (!trace) { return; }
    var_currentItemChanged(this, ui->tblTraceVars, current, tblTraceVars_old_col_name, "trace", trace->variables, trace->getTableName());
}

