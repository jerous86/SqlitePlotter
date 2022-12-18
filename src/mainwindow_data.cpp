//mainwindow stuff for the data table

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QRegularExpression>


QList<QRegularExpression> prepare_filters(const QString &filter) {
	QList<QRegularExpression> ret;
	for(auto &x : filter.split(" ")) {
		if (x.trimmed().length()>0) { ret<<QRegularExpression(x.trimmed(),QRegularExpression::CaseInsensitiveOption); }
	}
	return ret;
}

bool matches(const QList<QRegularExpression> &filters, const QString &txt) {
	for(const auto &filter : filters) {
		Q_ASSERT(filter.pattern().length()>0);
		if (filter.match(txt).hasMatch()) { return true; }
	}
	return false;
}

void MainWindow::updateTableAverages() {
	QTableWidget *tbl=ui->tblData;
	const auto N=tbl->rowCount();
	const auto nCols=tbl->columnCount();

	QVector<double> sum(nCols);
	unsigned visRowCount=0;

	for(int row_idx=0; row_idx<N; row_idx++) {
		if (tbl->isRowHidden(row_idx)) { continue; }
		visRowCount++;

		for(int col_idx=0; col_idx<nCols; col_idx++) {
			QVariant value=tbl->item(row_idx, col_idx)->data(Qt::DisplayRole);
			sum[col_idx]+=value.toDouble();
		}
	}

	for(int col_idx=0; col_idx<nCols; col_idx++) {
		tblDataColAvg[col_idx]->setText(QString::number(sum[col_idx]/visRowCount));
	}
}

void MainWindow::on_btnQueryData_clicked() {
	GET_SEL_T1;
	if (!trace) { return; }

	ui->txtData->setVisible(false);
	ui->tblData->setVisible(true);


	QTableWidget *tbl=ui->tblData;

	tbl->clear();
	tbl->setColumnCount(0);
	tbl->setRowCount(0);


	qDebug()<<"on_btnQueryData_clicked "<<trace->title;
	const QStringList col_names=trace->column_names(DoUseTraceQuery, ui->chkUngroup->isChecked() ? Ungroup : NoUngroup, EmptyOverride);
	const Trace::Rows *rows=trace->data(QStringList(), DoUseTraceQuery, ui->chkUngroup->isChecked() ? Ungroup : NoUngroup, EmptyOverride);
	if (!rows) { return; }

	tbl->setColumnCount(int(col_names.size()));
	tbl->setRowCount(int(rows->size()));
	tbl->setHorizontalHeaderLabels(col_names);

	while (tblDataColFilters.size()<col_names.length()) {
		{
			QLineEdit *wg=new QLineEdit();
			wg->setClearButtonEnabled(true);
			tblDataColFilters<<wg;
			tblDataHeadersLayout->addWidget(wg);
			connect(wg, SIGNAL(editingFinished()), this, SLOT(filterTableRows()));
		}
		{
			QLabel *wg=new QLabel();
			wg->setMaximumHeight(20);
			tblDataColAvg<<wg;
			tblDataHeadersAvgLayout->addWidget(wg);
		}
	}
	while (tblDataColFilters.size()>col_names.length()) {
		Q_ASSERT(tblDataColFilters.size()==tblDataHeadersLayout->count());
		Q_ASSERT(tblDataHeadersLayout->takeAt(tblDataColFilters.size()-1));
		delete tblDataHeadersLayout->takeAt(tblDataColFilters.size()-1);
		delete tblDataColFilters.back();
		tblDataColFilters.pop_back();
		Q_ASSERT(tblDataColFilters.size()==tblDataHeadersLayout->count());


		Q_ASSERT(tblDataColAvg.size()==tblDataHeadersAvgLayout->count());
		Q_ASSERT(tblDataHeadersAvgLayout->takeAt(tblDataColAvg.size()-1));
		delete tblDataHeadersAvgLayout->takeAt(tblDataColAvg.size()-1);
		delete tblDataColAvg.back();
		tblDataColAvg.pop_back();
		Q_ASSERT(tblDataColAvg.size()==tblDataHeadersAvgLayout->count());
	}
	Q_ASSERT(tblDataColFilters.size()==tblDataHeadersLayout->count());
	Q_ASSERT(tblDataColAvg.size()==tblDataHeadersAvgLayout->count());

	const int x=int(col_names.indexOf(trace->x.toUpper()));
	const int y=int(col_names.indexOf(trace->y.toUpper()));
	const int z=int(col_names.indexOf(trace->z.toUpper()));

	if (rows->size()==0) { return; }

	for(int row_idx=0; row_idx<rows->size(); row_idx++) {
		const QList<QVariant> &row=(*rows)[row_idx];
		for(int col_idx=0; col_idx<row.size(); col_idx++) {
			// Do *not* convert row to a string. It will not sort correctly.
			// QTableWidgetItem *wg=new QTableWidgetItem(row[col_idx]->toString());
			// Rather do:
			QTableWidgetItem *wg=new QTableWidgetItem();
			wg->setData(Qt::DisplayRole, row[col_idx]);

			tbl->setItem(row_idx,col_idx,wg);

			if (col_idx==x) { wg->setBackground(QColor(Qt::red).lighter());
			} else if (col_idx==y) { wg->setBackground(QColor(Qt::blue).lighter());
			} else if (col_idx==z) { wg->setBackground(QColor(Qt::green).lighter());
			}
		}
	}

	updateTableAverages();

	if (x>=0 && y>=0) {
		if (y<x) {
			tbl->horizontalHeader()->moveSection(y,1);
			tbl->horizontalHeader()->moveSection(x,0);
		} else {
			tbl->horizontalHeader()->moveSection(x,0);
			tbl->horizontalHeader()->moveSection(y,1);
		}
	}
	on_txtQueryColumnNameFilter_textChanged("");
	ui->tabs->setTabText(3, QString("Data (%1)").arg(rows->size()));
	filterTableRows();
}

void exportTraceData(QTextStream &out, const Trace *trace, QString columnFilter, bool ungroup) {
	const QStringList col_names=trace->column_names(DoUseTraceQuery, ungroup ? Ungroup : NoUngroup, EmptyOverride);
	const Trace::Rows *rows=trace->data(QStringList(), DoUseTraceQuery, ungroup ? Ungroup : NoUngroup, EmptyOverride);
	const bool show_all=columnFilter.trimmed().isEmpty();

	QList<bool> show;
	{
		const int x=int(col_names.indexOf(trace->x.toUpper()));
		const int y=int(col_names.indexOf(trace->y.toUpper()));
		const int z=int(col_names.indexOf(trace->z.toUpper()));

		const QList<QString> col_filters=columnFilter.split(" ");
		for(int i=0; i<col_names.size(); i++) {
			bool ok=show_all || (i==x) || (i==y) || (i==z);
			for(const QString &filter : col_filters) {
				if (filter.trimmed().isEmpty()) { continue; }
				if (col_names[i].contains(QRegularExpression(filter,QRegularExpression::CaseInsensitiveOption))) { ok=true; break; }
			}
			show<<ok;
		}
	}

	{
		QStringList xs;
		for(int i=0; i<col_names.size(); i++) { if (show[i]) { xs<<col_names[i]; } }
		out << xs.join("\t")<<"\n"; // Write header
	}

	for(int row_idx=0; row_idx<rows->size(); row_idx++) {
		QStringList values;
		const QList<QVariant> &row=(*rows)[row_idx];
		for(int col_idx=0; col_idx<row.size(); col_idx++) { if (show[col_idx]) { values<<row[col_idx].toString(); } }
		out << values.join("\t") << "\n";
	}
}

void MainWindow::on_btnExportData_clicked() {
	GET_SEL_T1;
	if (!trace) { return; }

	ui->txtData->setVisible(true);
	ui->tblData->setVisible(false);

	QString output;
	QTextStream out(&output, QIODevice::WriteOnly);
	exportTraceData(out, trace, ui->txtQueryColumnNameFilter->text(), ui->chkUngroup->isChecked());
	ui->txtData->setPlainText(output);
}

void MainWindow::on_txtQueryColumnNameFilter_textChanged(const QString &) {
	GET_SEL_T1;
	if (!trace) { return; }

	QTableWidget *tbl=ui->tblData;
	const auto col_filters=prepare_filters(ui->txtQueryColumnNameFilter->text());
	const bool show_all=ui->txtQueryColumnNameFilter->text().trimmed().isEmpty();

	QStringList col_names;
	for(int i=0; i<tbl->columnCount(); i++) { col_names<<tbl->horizontalHeaderItem(i)->text(); }

	const int x=int(col_names.indexOf(trace->x.toUpper()));
	const int y=int(col_names.indexOf(trace->y.toUpper()));
	const int z=int(col_names.indexOf(trace->z.toUpper()));

	for(int i=0; i<col_names.size(); i++) {
		bool ok=show_all || (i==x) || (i==y) || (i==z) || matches(col_filters, col_names[i]);
		tbl->setColumnHidden(i, !ok);
	}

	tbl->resizeColumnsToContents();
}
void MainWindow::on_txtQueryColumnNameFilter_editingFinished() { filterTableRows(); }
void MainWindow::invalidateAlignedLayout() { tblDataHeadersLayout->invalidate(); tblDataHeadersAvgLayout->invalidate(); }

#include <QStyledItemDelegate>
#include <QLocale>

void MainWindow::filterTableRows() {
	QList<QList<QRegularExpression>> filters;
	QTableWidget *tbl=ui->tblData;
	QStyledItemDelegate style;
	QLocale locale;
	// Only add filters whose column is visible -- that is more predictable behavior.
	for(int i=0; i<tbl->columnCount(); i++) {
		filters<<(tbl->isColumnHidden(i) ? QList<QRegularExpression>() : prepare_filters(tblDataColFilters[i]->text()));
	}

	for(int row=0; row<ui->tblData->rowCount(); row++) {
		bool visible=true;
		for(int col=0; col<filters.length(); col++) {
			if (filters[col].length()==0) { continue; }
			const QTableWidgetItem *item=ui->tblData->item(row,col);
			// https://forum.qt.io/post/350440
			// Doubles get formatted different than QVariant::toString ...
			const QString item_text=style.displayText(item->data(Qt::DisplayRole),locale);
			visible&=matches(filters[col], item_text);
		}
		ui->tblData->setRowHidden(row, !visible);
	}
	updateTableAverages();
}

