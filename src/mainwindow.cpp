#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <math.h>

#include <QDir>
#include <QDebug>
#include <QFileDialog>
//#include <QDialog>
#include <QInputDialog>
#include <QScrollBar>
#include <QMessageBox>
#include <QLabel>
#include <QDateTime>
#include <QRegularExpression>
#include <QClipboard>

#include "batchsave.h"
#include "plot_myplotter.h"
#include "misc.h"


#define DIRTY_EDITOR "dirty"
#define OLD_DATA	"old_data"

#define KEY_SETTINGS_HISTORY "settings-history"

const MapQStringQString EmptyOverride=MapQStringQString();

QStringList oldValues(const MainWindow *main, const QString &scope, const QString &var, bool toUrl, const int maxStringLength, const int maxEntries);
QStringList uniqueValues(const MainWindow *main, const QString &table, const QString &scope, const QString &var, bool toUrl, const int maxStringLength, const int maxEntries);


QVector<MainWindow *> MainWindow::instances;
QString lastMsg;
void log(const QString &str, const QColor &color) {
	for(MainWindow *win:MainWindow::instances) {
		if (win->hasFocus() || MainWindow::instances.size()==1) { win->log(str, color); }
	}
}

Trace *pos_to_trace(Set &set, const QList<QVariant> &pos) {
	if (pos.count()==2) {
		const int p=pos[0].toInt(), t=pos[1].toInt();
		return (p>=0 && p<set.plots.size() && t>=0 && t<set.plots[p]->traces.size())
				? set.plots[p]->traces[t]
				: nullptr;
	}
	return nullptr;
}
Plot *pos_to_plot(Set &set, const QList<QVariant> &pos, bool include_children) {
	if (pos.count()==1 || (include_children && pos.count()==2)) {
		const int p=pos[0].toInt();
		return set.plots[p];
	}
	return nullptr;
}

#include "plot_task.h"

FocusWatcher::FocusWatcher(QObject* parent) : QObject(parent) {
	Q_ASSERT(parent);
	parent->installEventFilter(this);
}

bool FocusWatcher::eventFilter(QObject *obj, QEvent *event) {
	Q_UNUSED(obj)
	if (event->type() == QEvent::FocusIn) emit focusChanged(true);
	else if (event->type() == QEvent::FocusOut) emit focusChanged(false);
	return false;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), db(Db()), set(db) {
	ui->setupUi(this);
	instances<<this;

	initializing=true;
	ui->splitterLeft->setSizes(QList<int>()<<1<<1);
	ui->txtData->setVisible(false);

    hlPlotDefaultQuery=new MySqliteHighlighter(ui->txtPlotDefaultQuery);
    hlTraceQuery=new MySqliteHighlighter(ui->txtTraceQuery);
    hlFinalQuery=new MySqliteHighlighter(ui->txtFinalQuery);

	LOG_INFO("CWD: "<<QDir::currentPath());

	if (traceStyles.size()==0) { init_trace_styles(); }

	ui->btnStopSaveImages->setVisible(false);
//	statusBar()->addWidget(ui->lblUniqueValues,1);

	{
		QList<QTableWidget *> tbls;
		tbls<<ui->tblGlobalVars<<ui->tblPlotVars<<ui->tblTraceVars;
		for(QTableWidget *tbl : tbls) {
			tbl->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(tbl, &QTableWidget::customContextMenuRequested, this, &MainWindow::handleVarsContextMenu);
		}
	}

	ui->btnDatabase->setMenu(&menuFile);
	ui->btnSettingsFile->setMenu(&menuSettings);
	ui->btnCheck->setMenu(&menuCheck);
	connect(menuCheck.addAction("Check with data"), SIGNAL(triggered()), SLOT(_on_btnCheckWithData_clicked()));
	connect(menuCheck.addAction("Uncheck all"), SIGNAL(triggered()), SLOT(_on_btnUncheckAll_clicked()));
	connect(menuCheck.addAction("Check all"), SIGNAL(triggered()), SLOT(_on_btnCheckAll_clicked()));

	ui->btnExportMenu->setMenu(&menuExport);
	connect(menuExport.addAction("To png ..."), SIGNAL(triggered()), SLOT(_on_btnSaveImages_clicked()));
	connect(menuExport.addAction("To text files ..."), SIGNAL(triggered()), SLOT(_on_btnSaveData_clicked()));

	{
		auto wg=ui->tblData;
		ui->layoutTableHeaders->addLayout(tblDataHeadersLayout=new ColumnAlignedLayout());
		ui->layoutTableHeaders->addLayout(tblDataHeadersAvgLayout=new ColumnAlignedLayout());
		tblDataHeadersLayout->setTableColumnsToTrack(wg->horizontalHeader());
		tblDataHeadersAvgLayout->setTableColumnsToTrack(wg->horizontalHeader());
		connect(wg->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), SLOT(invalidateAlignedLayout()));
		connect(wg->horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(invalidateAlignedLayout()));
	}


	// We place on_btnPlot_clicked before the restoreState call, because 1) we don't want
	// to generate plots on load (if the image takes a while to generate, it will also
	// take a while before we see any interface) and 2) on_btnPlot_clicked() generates
	// the QList<QLabel *> img, which we certainly need!
	// NOTE restoreState implicitly calls on_btnPlot_clicked(), so it is commented out on the next line.
	//	on_btnPlot_clicked();
	restoreState();

	refresh_menuFile();
	refresh_menuSettings();

    ui->chkLegend->setChecked(plotOptions.plotLegend);
    ui->chkTitle->setChecked(plotOptions.plotTitle);

	// Only save on losing focus. A QPlainText does not offer a editingFinished signal.
	QList<QTextEdit *> editors;
	editors<<ui->txtPlotDefaultQuery<<ui->txtTraceQuery;
	for(QTextEdit *wg:editors) {
		// NOTE: memory leak
		FocusWatcher *focusWatcher=new FocusWatcher(wg);
		connect(focusWatcher, &FocusWatcher::focusChanged, [this,wg](bool in) {
			if (in) {
				wg->property(OLD_DATA).setValue(wg->toPlainText());
			} else {
				if (wg->property(DIRTY_EDITOR).toBool()==true) { _editingFinished(*wg); }
			}});
	}

    QList<QWidget*> hide;
    hide
        <<ui->chkWinSync <<ui->btnResizeWindows <<ui->chkKeepAR <<ui->cmbPlotMode
        <<ui->lblGrid<<ui->txtAltColumns<<ui->txtAltRows
		<<ui->btnMulticlone2
        <<ui->chkStableColors<<ui->chkBBox<<ui->chkD3Grid
        <<ui->txtD3GridMesh<<ui->txtXrot<<ui->txtZrot
        <<ui->txtZAxisLabel<<ui->chkLogZ<<ui->txtZMin<<ui->txtZMax
        <<ui->chkXTic<<ui->chkYTic<<ui->chkZTic
          ;
    for(QWidget *wg:hide) { wg->setVisible(false); }

	ui->tree->setFocus();

	initializing=false;
	on_btnPlot_clicked();
}

MainWindow::~MainWindow() {
	instances.removeAll(this);
	storeState();
	deinit_trace_styles();
	delete ui;
}

QByteArray serialize(const QMap<QString,QStringList> &xs) {
	QByteArray data;
	QDataStream stream(&data, QIODevice::WriteOnly);
	stream<<xs;
	return data;
}

template<class T> T deserialize(QByteArray &data);

template<>
QMap<QString,QStringList> deserialize(QByteArray &data) {
	QDataStream stream(&data, QIODevice::ReadOnly);
	QMap<QString,QStringList> ret;
	stream>>ret;
	return ret;
}

// If false, then we will not change the db_file and settings_file in MainWindow::restoreState.
// This is needed when we have SyncWindows checked, and we are using different database files!
bool restoreDbFileAndSettingsFile=true;

void MainWindow::storeState() {
	{
		QSETTINGS_GLOBAL;
		{ QStringList history=settings.value("history").toStringList();
			settings.setValue("history", history); }

		{ QStringList history=settings.value(KEY_SETTINGS_HISTORY).toStringList();
			settings.setValue(KEY_SETTINGS_HISTORY, history); }
	}

    QSETTINGS;
	settings.setValue("mainWindowGeometry", saveGeometry());
	settings.setValue("mainWindowState", saveState());
	settings.setValue("isMaximized", isMaximized());
	settings.setValue("pos", pos());
	settings.setValue("size", size());
	settings.setValue("splitterLeftRight", ui->splitterLeftRight->saveState());
	settings.setValue("splitterLeft", ui->splitterLeft->saveState());

	QStringList sel;
	for(auto s:sel_to_pos(ui->tree)) {
		if (s.size()==1) { sel<<QString("%1").arg(s[0].toInt());
		} else if (s.size()==2) { sel<<QString("%1 %2").arg(s[0].toInt()).arg(s[1].toInt());
		} else {Q_ASSERT(false); }
	}
	settings.setValue("selection", sel);
	settings.setValue("used_values", serialize(used_values));

	set.store(settings);


	QList<QTextEdit *> editors;
	editors<<ui->txtPlotDefaultQuery<<ui->txtTraceQuery;
	for(auto *e:editors) { e->setProperty(DIRTY_EDITOR, false); }

	if (ui->chkWinSync->isChecked()) {
		restoreDbFileAndSettingsFile=false;
		for(auto *instance:instances) {
			if (instance==this) { continue; }
            QSETTINGS;
			instance->set.restore(settings);
			instance->post_xdo(/*keep_sel*/true);
			instance->on_btnPlot_clicked();
		}
		restoreDbFileAndSettingsFile=true;
	}
}


void MainWindow::restoreState() {
	restoringState=true;
	clear_history();
	if (restoreDbFileAndSettingsFile) {
		QSETTINGS_GLOBAL;
		{ QStringList history=settings.value("history").toStringList();
            if (history.size()>0) {  db.dbFiles=history.first().split(DB_FILES_SPLIT_CHAR); } }

		{ QStringList history=settings.value(KEY_SETTINGS_HISTORY).toStringList();
            if (history.size()>0) {  settings_file=history.first(); } }
	}
	LOG("Global settings are stored in "<<QSETTINGS_GLOBAL_PATH)
    LOG("Local settings are stored in "<<QSETTINGS_LOCAL_PATH(this))


    QSETTINGS;
	//hide();
	restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
	QMainWindow::restoreState(settings.value("mainWindowState").toByteArray());
	const bool isMaximized=settings.value("isMaximized").toBool();
	const QSize size=settings.value("size").toSize();
	const QPoint pos=settings.value("pos").toPoint();
	if (isMaximized) { showMaximized(); } else { show(); }
	ui->splitterLeftRight->restoreState(settings.value("splitterLeftRight").toByteArray());
	ui->splitterLeft->restoreState(settings.value("splitterLeft").toByteArray());
	QMainWindow::move(pos);
	QMainWindow::resize(size);

	QByteArray data=settings.value("used_values").toByteArray();
	used_values=deserialize<QMap<QString,QStringList>>(data);
//	qDebug()<<"restoreState: used_values "<<data;
//	qDebug()<<"restoreState: used_values "<<used_values;

	set.db.close_db();
	set.db.open_db();
	set.restore(settings);

	const bool autoReplot=set.autoReplot;
	set.autoReplot=false; // don't replot while loading
	ui->tree->blockSignals(true);
	for(QVariant sel:settings.value("selection").toStringList()) {
		QList<QVariant> pos;
		for(auto x:sel.toString().split(" ")) { pos<<QVariant(x.toInt()); }
		auto *wg=pos_to_item(ui->tree, pos);
		if (wg) { wg->setSelected(true); }
	}
	ui->tree->blockSignals(false);
	set.autoReplot=autoReplot;


	on_db_reload();
	post_xdo(false);

    setCustomWindowTitle("", "");

	// This must be placed last, as it might result in storeState being called.
	on_tree_itemSelectionChanged();

	restoringState=false;
}


void MainWindow::setCustomWindowTitle(const QString &extra, const QString &table) {
	setWindowTitle(QString("SqlitePlotter [%1] %2 (%3 rows in table %4) [last reload %5]")
               .arg(set.db.dbFiles.size()==1
				   ? QFileInfo(set.db.dbFiles.join(DB_FILES_SPLIT_CHAR)).fileName()
				   : QString("%1 files").arg(set.db.dbFiles.size())
				   )
		       .arg(extra)
               .arg(n_total_rows).arg(table)
		       .arg(QDateTime::currentDateTime().toString())
		       );
}

void MainWindow::log(const QString &str, const QColor &color) {
	if (str.length()>0) { lastMsg=str; }
	ui->txtLog->appendHtml(QString("<p style='color:%2'>%1</p>").arg(str).arg(color.name())
			       .replace("\t",QString("&nbsp;").repeated(4)));

	logCounter++;
	ui->tabs->setTabText(4, QString("Log (%1)").arg(logCounter));
}

void MainWindow::add_used_value(const QString &var, const QString &value) {
	used_values[var].removeAll(value);
	used_values[var].push_front(value);
	while (used_values[var].size()>10) { used_values[var].pop_back(); }

	{
        QSETTINGS;
		settings.setValue("used_values", serialize(used_values));
	}
}


void MainWindow::on_btnClearLog_clicked() {
	logCounter=0;
	ui->tabs->setTabText(4, QString("Log (%1)").arg(logCounter));
	ui->txtLog->setPlainText("");
}

QString humanFileSize(int bytes, QLocale &locale) {
#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
	return locale.formattedDataSize(bytes);
#else
    (void) locale;
	return QString("%1 KiB").arg(bytes/1024);
#endif
}

void MainWindow::refresh_menuFile() {
	QLocale locale = this->locale();
	menuFile.clear();

	{
		QAction *action=menuFile.addAction(QString("Reload\t%1").arg(QKeySequence(QKeySequence::Refresh).toString()));
		action->setShortcut(QKeySequence::Refresh);
		connect(action, &QAction::triggered, [this]() {
			clear_history();
			set.db.close_db();
			set.db.open_db();
			on_db_reload();
			if (ui->chkAutoReplot->isChecked()) { on_btnPlot_clicked(); }
		});
	}

    QStringList allFiltersList=QStringList()<<"sqlite3 (*.sqlite3)";
    QStringList allSupported=QStringList()<<"*.sqlite3";
    for(Importer *importer : importers) {
        allFiltersList<<importer->name()+" ("+importer->patterns().join(" ")+")";
        allSupported<<importer->patterns();
    }
    allFiltersList.insert(0, "Supported files ("+allSupported.join(" ")+")");
    const QString allFilters=allFiltersList.join(";;");

    menuFile.addSeparator();
	{
		QAction *action=menuFile.addAction(QString("Open file(s) ... \t%1").arg(QKeySequence(QKeySequence::Open).toString()));
		action->setShortcut(QKeySequence::Open);
        connect(action, &QAction::triggered, [this,allFilters]() {
            const QStringList new_dbs=QFileDialog::getOpenFileNames(this,
              "Select database files", "", allFilters,
			  nullptr, QFileDialog::DontResolveSymlinks);
            if (new_dbs.length()==0) { return; }
            loadFiles(new_dbs);
		});
	}
    {
        QMenu &menu=*menuFile.addMenu("Load recent file");
        QSETTINGS_GLOBAL;
        QStringList history=settings.value("history").toStringList();
        for(QString &f:history) {
            if (f.trimmed().length()==0) { continue; }
            QFileInfo fi(f);
            QAction *action=menu.addAction(QString("%1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,f]() {
                loadFiles(f.split(DB_FILES_SPLIT_CHAR));
            });
        }
        {
            QAction *action=menu.addMenu("Clear history")->addAction("Confirm");
            connect(action, &QAction::triggered, [this]() {
                QSETTINGS_GLOBAL;
                settings.setValue("history", QStringList());
                refresh_menuFile();
            });
        }
    }

    QFileInfo dir=QFileInfo(set.db.dbFiles.size()==0 ? "" : set.db.dbFiles[0]);
    const QStringList mfiles=dir.dir().entryList(allSupported, QDir::NoFilter, QDir::Time);
    QStringList files=mfiles;
    std::sort(files.rbegin(), files.rend());
    {
        QMenu &menu=*menuFile.addMenu(QString("Load from %1").arg(dir.absolutePath()));

        for(QString f : files) {
            QFileInfo fi(dir.absolutePath()+"/"+f);
            QAction *action=menu.addAction(QString("%1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,fi]() { loadFile(fi); });
        }
    }
    menuFile.addSeparator();
    {
		QAction *action=menuFile.addAction(QString("Open additional file(s) ..."));
		action->setShortcut(QKeySequence::Open);
        connect(action, &QAction::triggered, [this,allFilters]() {
            const QStringList new_dbs=QFileDialog::getOpenFileNames(this,
              "Select database files", "", allFilters,
			  nullptr, QFileDialog::DontResolveSymlinks);
            if (new_dbs.length()==0) { return; }
            loadAdditionalFiles(new_dbs);
		});
	}
    {
        QMenu &menu=*menuFile.addMenu("Load additional recent file");
        QSETTINGS_GLOBAL;
        QStringList history=settings.value("history").toStringList();
        for(QString &f:history) {
            if (f.trimmed().length()==0) { continue; }
            QFileInfo fi(f);
            QAction *action=menu.addAction(QString("%1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,f]() {
                loadAdditionalFiles(f.split(DB_FILES_SPLIT_CHAR));
            });
        }
        {
            QAction *action=menu.addMenu("Clear history")->addAction("Confirm");
            connect(action, &QAction::triggered, [this]() {
                QSETTINGS_GLOBAL;
                settings.setValue("history", QStringList());
                refresh_menuFile();
            });
        }
    }
    {
        QMenu &menu=*menuFile.addMenu(QString("Open additional file from %1").arg(dir.absolutePath()));

        for(QString f : files) {
            QFileInfo fi(dir.absolutePath()+"/"+f);
            QAction *action=menu.addAction(QString("%1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,fi]() { loadAdditionalFiles(QStringList(fi.absoluteFilePath())); });
        }
    }

    menuFile.addSeparator();
    {
		QAction *action=menuFile.addAction(QString("Save to sqlite3 file ... \t%1").arg(QKeySequence(QKeySequence::SaveAs).toString()));
		action->setShortcut(QKeySequence::SaveAs);
		connect(action, &QAction::triggered, [this]() {
			const QString tgtFilePath=QFileDialog::getSaveFileName(this,
                                           "Select database file to save to", QFileInfo(db.dbFiles[0]).path(),
									       "sqlite3 (*.sqlite3)");
			if (tgtFilePath.length()==0) { return; }
			db.save_db(tgtFilePath);
		});
	}
	menuFile.addSeparator();
	{
		QAction *action=menuFile.addAction(QString("Copy current path to clipboard"));
		connect(action, &QAction::triggered, [this]() {
			QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(db.dbFiles.join(DB_FILES_SPLIT_CHAR));
		});
	}


	menuFile.addSeparator();
	{
		if (mfiles.size()>0) {
			const QString f=mfiles.front();
			QFileInfo fi(dir.absolutePath()+"/"+f);
			QAction *action=menuFile.addAction(QString("Load most recently modified file: %1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,fi]() { loadFile(fi); });
		} else {
			menuFile.addAction(QString("No MRU"))->setEnabled(false);
		}
        const int i=int(files.indexOf(QFileInfo(db.dbFiles.size()==0?"":db.dbFiles[0]).fileName()));
		if (i>0) {
			const QString f=files[i-1];
			QFileInfo fi(dir.absolutePath()+"/"+f);
			QAction *action=menuFile.addAction(QString("Load previous sibling: %1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,fi]() { loadFile(fi); });
		} else {
			menuFile.addAction(QString("No previous sibling"))->setEnabled(false);
		}
		if (i<files.size()-1) {
			const QString f=files[i+1];
			QFileInfo fi(dir.absolutePath()+"/"+f);
			QAction *action=menuFile.addAction(QString("Load next sibling: %1 (%2)").arg(f).arg(humanFileSize(fi.size(),locale)));
            connect(action, &QAction::triggered, [this,fi]() { loadFile(fi); });
		} else {
			menuFile.addAction(QString("No next sibling"))->setEnabled(false);
		}
	}
}

void MainWindow::refresh_menuSettings() {
	QMenu &menu=menuSettings;
	menu.clear();

	{
		QAction *action=menu.addAction("Open file ...");
		connect(action, &QAction::triggered, [this]() {
			QString settings_file2=QFileDialog::getOpenFileName(this, "Select a ini settings files", QFileInfo(settings_file).absolutePath(), "sqliteplotter settings (sqliteplotter.*.ini);;All files (*.*)");
			if (settings_file2.length()==0) { return; }

			qDebug()<<"refreshMenuSettings::Open file "<<settings_file2<<"::triggered";
			clear_history();
			storeState();
            setSettingsFile(settings_file2);
			refresh_menuSettings();
		});
	}
	{
		QAction *action=menu.addAction("New file ...");
		connect(action, &QAction::triggered, [this]() {
			QString settings_file2=QFileDialog::getSaveFileName(this, "Select a ini settings files", "sqliteplotter.settings.ini");
			if (settings_file2.length()==0) { return; }

			clear_history();
            setSettingsFile(settings_file2);
			refresh_menuSettings();
		});
	}
	menu.addSeparator();

	{
		QSETTINGS_GLOBAL;
		QStringList history=settings.value(KEY_SETTINGS_HISTORY).toStringList();
		for(QString &f:history) {
			if (f.trimmed().length()==0) { continue; }
			QAction *action=menu.addAction(QString("Load %1").arg(f));
			connect(action, &QAction::triggered, [this,f]() {
				qDebug()<<"refreshMenuSettings::load "<<f<<"::triggered";
				clear_history();
				storeState();
                setSettingsFile(f);
				refresh_menuSettings();
			});
		}
	}

	{
		QAction *action=menu.addMenu("Clear history")->addAction("Confirm");
		connect(action, &QAction::triggered, [this]() {
			QSETTINGS_GLOBAL;
			settings.setValue(KEY_SETTINGS_HISTORY, QStringList());
			refresh_menuSettings();
		});
	}
}

void MainWindow::refresh_cmbTraceType() {
	ui->cmbTraceType->blockSignals(true);

	ui->cmbTraceType->clear();
	const bool x=ui->cmbTraceX->currentText().length()>0;
	const bool y=ui->cmbTraceY->currentText().length()>0;
	const bool z=ui->cmbTraceZ->currentText().length()>0;
	const int dim=(x && y && z ? 3 : (x && y ? 2 : (x ? 1 : 0)));
	ui->cmbTraceType->addItem("");
	for(auto *type:traceStyles) {
		if (type->supports_dim(dim)) { ui->cmbTraceType->addItem(type->name()); }
	}

	ui->cmbTraceType->blockSignals(true);
}

void MainWindow::_editingFinished(QTextEdit &wg) {
	if (&wg==ui->txtPlotDefaultQuery) { _on_txtPlotDefaultQuery_editingFinished();
	} else if (&wg==ui->txtTraceQuery) { _on_txtTraceQuery_editingFinished();
	} else { Q_ASSERT(false);
	}
}

void MainWindow::on_chkStableColors_toggled(bool) {
	//	push_undo();
	set.stableColors=ui->chkStableColors->isChecked();
	on_update(true, NoRefresh);
}

void MainWindow::on_chkBBox_toggled(bool) {
	set.bbox=ui->chkBBox->isChecked();
	on_update(true, NoRefresh);
}


void MainWindow::setSettingsFile(const QString &f) {
	{
		QSETTINGS_GLOBAL;
		QStringList history=settings.value(KEY_SETTINGS_HISTORY).toStringList();
		history.removeAll(f);
		history.insert(0, f);
		settings.setValue(KEY_SETTINGS_HISTORY, history);
	}
	restoreState();
}



void MainWindow::_on_btnSaveImages_clicked() {
    QSETTINGS;
	GET_SEL_P(false);

	BatchSave win(this, plots,
		      settings.value("lastBatchSavePatternPng", "${DB_ROOT}/gfx/${PLOT_INDEX}.png").toString(),
              QFileInfo(db.dbFiles[0]).absolutePath());
	win.exec();
	if (win.accept_state!=1) { return; }


	settings.setValue("lastBatchSavePatternPng", win.getText());
	const QStringList lines=win.lines();
	const QString pattern=win.pattern(lines);
	const QMap<QString,QStringList> cp_vars=win.cp_vars(lines);
	const unsigned n_combinations=win.n_combinations(cp_vars);
	const unsigned total=n_combinations*unsigned(plots.size());

	const QString info=QString("Generating %3 (=%1 combinations * %2 plots) images")
			.arg(n_combinations).arg(plots.size()).arg(total);
	DEBUG(info);
	/*if (n_combinations*plots.size()>10 && QMessageBox::question(this,
		info,
		QString("Confirm: %1").arg(info))==QMessageBox::No) {
		return;
	}*/

	ui->btnExportMenu->setVisible(false);
//	ui->btnSaveImages->setVisible(false);
	ui->btnStopSaveImages->setVisible(true);
	ui->btnStopSaveImages->setChecked(false);
	ui->tabs->setCurrentIndex(ui->tabs->indexOf(ui->tabLog));
	QCoreApplication::processEvents();

	LOG_INFO("");
	LOG_INFO(info);
	int counter=0;
	for(const Plot *plot : plots) {
		if (ui->btnStopSaveImages->isChecked()) { break; }
		for(unsigned i=0; i<n_combinations; i++) {
			if (ui->btnStopSaveImages->isChecked()) { break; }
			AspectRatioPixmapLabel *img=images[0];
			const MapQStringQString override_vars=win.nth_combination(cp_vars, i);
			const int plot_index=override_vars["PLOT_INDEX"].toInt();
			const QString tgt=QFileInfo(plot->apply_variables(pattern, override_vars)).absoluteFilePath();

			LOG_INFO("\t"<<round(100*double(counter++)/total)<<"%: Generating png with PLOT_INDEX="<<plot_index);
			DEBUG(override_vars);

			GeneratePlotTask task(this, plot_index);
            MyPlotter::invalidateCache(img);
            task.run_myPlotter(*plot, plotOptions, img, override_vars);
            img->update();
            QCoreApplication::processEvents();
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            img->pixmap().save(tgt);
#else
            img->pixmap()->save(tgt);
#endif
			QCoreApplication::processEvents();
		}
	}
	set.globalVars.remove("PLOT_INDEX");
	ui->btnExportMenu->setVisible(true);
//	ui->btnSaveImages->setVisible(true);
	ui->btnStopSaveImages->setVisible(false);
	LOG_INFO("Finished writing "<<(unsigned(plots.size())*n_combinations)<<" files!");
	LOG_INFO("");
}


void exportTraceData(QTextStream &out, const Trace *trace, QString columnFilter, bool ungroup);
void MainWindow::_on_btnSaveData_clicked() {
    QSETTINGS;
	GET_SEL_P(false);

	BatchSave win(this, plots,
		      settings.value("lastBatchSavePatternTsv", "${DB_ROOT}/tsv/${PLOT_INDEX}-${TRACE_INDEX}-${TSV_INDEX}.tsv").toString(),
              QFileInfo(db.dbFiles[0]).absolutePath());
	win.exec();
	if (win.accept_state!=1) { return; }


	settings.setValue("lastBatchSavePatternTsv", win.getText());
	const QStringList lines=win.lines();
	const QString pattern=win.pattern(lines);
	const QMap<QString,QStringList> cp_vars=win.cp_vars(lines);
	const unsigned n_combinations=win.n_combinations(cp_vars);
	const unsigned total=n_combinations*unsigned(plots.size());

	const QString info=QString("Generating %3 (=%1 combinations * %2 plots) files").arg(n_combinations).arg(plots.size()).arg(total);
	DEBUG(info);

	ui->btnExportMenu->setVisible(false);
//	ui->btnSaveImages->setVisible(false);
	ui->btnStopSaveImages->setVisible(true);
	ui->btnStopSaveImages->setChecked(false);
	ui->tabs->setCurrentIndex(ui->tabs->indexOf(ui->tabLog));
	QCoreApplication::processEvents();

	LOG_INFO("");
	LOG_INFO(info);
	unsigned counter=0;
	for(Plot *plot : plots) {
		for(Trace *trace : plot->traces) {
			if (ui->btnStopSaveImages->isChecked()) { break; }
			MapQStringQString override_vars=win.nth_combination(cp_vars, counter);
			override_vars["TSV_INDEX"]=QString::number(counter);
			override_vars["PLOT_INDEX"]=QString::number(plots.indexOf(plot));
			override_vars["TRACE_INDEX"]=QString::number(plot->traces.indexOf(trace));
			override_vars["PLOT_TITLE"]=plot->get_title(override_vars).replace("/","_").replace(" ","_");
			override_vars["TRACE_TITLE"]=trace->get_title(override_vars).replace("/","_").replace(" ","_");
			const QString filePath=trace->apply_variables(pattern, override_vars);
			LOG_INFO("\t\tExporting to "<<filePath);
			QFile data(filePath);
			data.open(QFile::WriteOnly);
			QTextStream out(&data);
			exportTraceData(out, trace, "", false);
			counter++;
		}
	}
	ui->btnExportMenu->setVisible(true);
//	ui->btnSaveImages->setVisible(true);
	ui->btnStopSaveImages->setVisible(false);
}




void MainWindow::handleVarsContextMenu(const QPoint &pos) {
	QTableWidget *tbl=dynamic_cast<QTableWidget *>(sender());
	QTableWidgetItem *item = tbl->itemAt(pos);
	if (!item) { return; }
	item=tbl->item(item->row(), 0);
	Q_ASSERT(item);

	const QString var=item->text();
    QString scope, table;
    if (tbl==ui->tblGlobalVars) {
        scope="global";
        table=DEFAULT_TABLE_NAME;
    } else if (tbl==ui->tblPlotVars) {
        scope="plot";
        GET_SEL_P1(true);
        if (plot) { table=plot->getTableName(); }
    } else if (tbl==ui->tblTraceVars) {
        scope="trace";
        GET_SEL_T1;
        if (trace) { table=trace->getTableName(); }
    } else { Q_ASSERT(false);
	}

	QMenu *m=new QMenu();
	m->addAction("Previous values")->setEnabled(false);
	const QStringList xs=oldValues(this, scope, var, false, 0, 50);
	for(auto &value:xs) {
		QAction *a=m->addAction(value);
		connect(a, &QAction::triggered, [this,scope,var,value]() { on_lblUniqueValues_linkActivated(QString("%1 §§ %2 §§ %3").arg(scope).arg(var).arg(value)); });
	}
	{
		QMenu *ma=m->addMenu("Clear old values");
		QAction *a=ma->addAction("Yes, clear old values");
		QString value="__CMD__";
		connect(a, &QAction::triggered, [this,scope,var,value]() { on_lblUniqueValues_linkActivated(QString("%1 §§ %2 §§ %3 §§ clear").arg(scope).arg(var).arg(value)); });
	}
	QMenu *ma=m->addMenu("Unique values");
    for(const QString &value:uniqueValues(this, table, scope, var, false, 0, 50)) {
		//        if (xs.contains(value)) { continue; } // only show values not shown before
		QAction *a=ma->addAction(value);
		connect(a, &QAction::triggered, [this,scope,var,value]() { on_lblUniqueValues_linkActivated(QString("%1 §§ %2 §§ %3").arg(scope).arg(var).arg(value)); });
	}
	m->popup(QCursor::pos());
}

void MainWindow::on_update(bool store, RefreshTreeMode refresh, bool autoReplotIfOk) {
	GET_SEL_T1;
	// Also modified in void MainWindow::on_tree_itemSelectionChanged()!
	ui->txtFinalQuery->setPlainText(QString(trace ? trace->get_query(EmptyOverride) : "").replace(QRegularExpression("[\\t \\n]+"), " "));
	ui->tblData->setRowCount(0);

    if (store) { storeState(); }
	if (refresh!=0) { refresh_tree(refresh==RefreshTreeKeepSel); }
	if (autoReplotIfOk && set.autoReplot) { on_btnPlot_clicked(); }
	if (ui->tabs->currentWidget()==ui->tabData) { on_btnQueryData_clicked(); }
}

void MainWindow::on_db_reload() {
    n_total_rows=0;
    QSet<QString> tables_seen;
    QStringList columnNames; // NOTE: we're lazily loading all column names here from all tables
        // and considering all columns valid, regardless of editor. Ideally, we would
        // update the editor, depending on the query ...
    for(const Plot *plot : set.plots) {
        const QString table=plot->getTableName();
        if (!tables_seen.contains(table)) {
            tables_seen.insert(plot->getTableName());
            n_total_rows += set.db.row_count(table);
			if (set.db.table_exists(table)) {
				for(const QString &col:set.db.query(QString("SELECT * FROM %1 LIMIT 1").arg(table)).column_names()) {
					if (!columnNames.contains(col)) {
						columnNames<<col;
					}
				}
			}
        }
    }

	Trace::invalidateCache();
    setCustomWindowTitle("", QStringList(tables_seen.values()).join(","));
    hlPlotDefaultQuery->setColumnNames(columnNames);
    hlTraceQuery->setColumnNames(columnNames);
    hlFinalQuery->setColumnNames(columnNames);

	unique_values.clear();
	//	used_values.clear();
	refresh_tree(true);
	refresh_lstGlobalVars();
	on_update();
}


void MainWindow::on_txtPlotDefaultQuery_textChanged() {
	ui->txtPlotDefaultQuery->setProperty(DIRTY_EDITOR, true);
}

void MainWindow::_on_txtPlotDefaultQuery_editingFinished() {
	push_undo("Set plot's","default query");
	GET_SEL_P(true);
	for(Plot *plot:plots) { plot->defaultQuery=ui->txtPlotDefaultQuery->toPlainText(); }
	on_update(true, RefreshTreeKeepSel);
}

#define ON_EDITING_FINISHED(OBJ, actualValue)	\
	QLineEdit *wg=OBJ; \
	if (!wg->isModified() || wg->text()==actualValue) { return; } \
	const QString value=wg->text(); \
	(void) 0

#define APPLY_PLOT_EDIT(UNDO_TITLE1, UNDO_TITLE2, PLOT_ACTION) \
	GET_SEL_P(true); \
	push_undo(UNDO_TITLE1, UNDO_TITLE2); \
	for(Plot *plot:plots) { PLOT_ACTION; } \
	on_update(true, NoRefresh); \
	(void) 0


void MainWindow::on_txtPlotTitle_editingFinished() {
	ON_EDITING_FINISHED(ui->txtPlotTitle, "--");
	APPLY_PLOT_EDIT("Edit plot's","title", plot->title=value);
}

void MainWindow::on_txtXAxisLabel_editingFinished() {
	ON_EDITING_FINISHED(ui->txtXAxisLabel, "--");
	APPLY_PLOT_EDIT("Edit","x-label", plot->xAxisLabel=value);
}
void MainWindow::on_txtYAxisLabel_editingFinished() {
	ON_EDITING_FINISHED(ui->txtYAxisLabel, "--");
	APPLY_PLOT_EDIT("Edit","y-label", plot->yAxisLabel=value);
}

void MainWindow::on_txtZAxisLabel_editingFinished() {
	ON_EDITING_FINISHED(ui->txtZAxisLabel, "--");
	APPLY_PLOT_EDIT("Edit","z-label", plot->zAxisLabel=value);
}


void MainWindow::on_chkD3Grid_toggled(bool) { APPLY_PLOT_EDIT("Toggle","d3dGrid", plot->d3Grid=ui->chkD3Grid->isChecked()); }

void MainWindow::on_txtD3GridMesh_editingFinished() { APPLY_PLOT_EDIT("Edit","mesh size", plot->d3Mesh=ui->txtD3GridMesh->value()); }
void MainWindow::on_txtD3GridMesh_valueChanged(int) { on_txtD3GridMesh_editingFinished(); }

void MainWindow::on_txtXrot_editingFinished() { APPLY_PLOT_EDIT("Edit","xrot", plot->d3Xrot=ui->txtXrot->value()); }
void MainWindow::on_txtXrot_valueChanged(int) { on_txtXrot_editingFinished(); }

void MainWindow::on_txtZrot_editingFinished() { APPLY_PLOT_EDIT("Edit","zrot", plot->d3Zrot=ui->txtZrot->value()); }
void MainWindow::on_txtZrot_valueChanged(int) { on_txtZrot_editingFinished(); }

void MainWindow::on_txtYMin_editingFinished() {
	ON_EDITING_FINISHED(ui->txtYMin, "--");
	APPLY_PLOT_EDIT("Edit","y-min", plot->ymin=value);
}
void MainWindow::on_txtYMax_editingFinished() {
	ON_EDITING_FINISHED(ui->txtYMax, "--");
	APPLY_PLOT_EDIT("Edit","y-max", plot->ymax=value);
}
void MainWindow::on_txtXMin_editingFinished() {
	ON_EDITING_FINISHED(ui->txtXMin, "--");
	APPLY_PLOT_EDIT("Edit","x-min", plot->xmin=value);
}
void MainWindow::on_txtXMax_editingFinished() {
	ON_EDITING_FINISHED(ui->txtXMax, "--");
	APPLY_PLOT_EDIT("Edit","x-max", plot->xmax=value);
}
void MainWindow::on_txtZMin_editingFinished() {
	ON_EDITING_FINISHED(ui->txtZMin, "--");
	APPLY_PLOT_EDIT("Edit","z-min", plot->zmin=value);
}
void MainWindow::on_txtZMax_editingFinished() {
	ON_EDITING_FINISHED(ui->txtZMax, "--");
	APPLY_PLOT_EDIT("Edit","z-max", plot->zmax=value);
}

void MainWindow::on_chkLogX_toggled() { APPLY_PLOT_EDIT("Toggle","x-log", plot->logX=ui->chkLogX->isChecked()); }
void MainWindow::on_chkLogY_toggled() { APPLY_PLOT_EDIT("Toggle","y-log", plot->logY=ui->chkLogY->isChecked()); }
void MainWindow::on_chkLogZ_toggled() { APPLY_PLOT_EDIT("Toggle","z-log", plot->logX=ui->chkLogZ->isChecked()); }

void MainWindow::on_txtAltColumns_valueChanged(int c) { APPLY_PLOT_EDIT("Set","alt-columns", plot->altColumns=c); }
void MainWindow::on_txtAltRows_valueChanged(int c) { APPLY_PLOT_EDIT("Set","alt-rows", plot->altRows=c); }


void MainWindow::on_tabs_currentChanged(int) {
	if (ui->tabs->currentWidget()==ui->tabData) { on_btnQueryData_clicked(); }
}

void MainWindow::on_txtTraceQuery_textChanged() {
	ui->txtTraceQuery->setProperty(DIRTY_EDITOR, true);
}

void MainWindow::_on_txtTraceQuery_editingFinished() {
	push_undo("Set","trace's query");
	GET_SEL_T;
	for(Trace *trace:traces) { trace->query=ui->txtTraceQuery->toPlainText(); }
	on_update(true, RefreshTreeKeepSel);
}

#define DO_ON_T_OR_P(CODE_TRACE) \
	for(Trace *trace:traces) { Q_ASSERT(trace); CODE_TRACE; } \
	for(Plot *plot:plots) { Q_ASSERT(plot); for(Trace *trace:plot->traces) { Q_ASSERT(trace); CODE_TRACE; } } \
	(void)0

// NOTE we indeed reuse VALUE. Sometimes we use new pointers, so we must create a new instance then ...
#define SET_FIELD_ON_T_OR_P(FIELD, VALUE)   DO_ON_T_OR_P(trace->FIELD=VALUE)


void MainWindow::on_txtTraceTitle_editingFinished() {
	ON_EDITING_FINISHED(ui->txtTraceTitle, "--");
	GET_SEL_PT(false);
	push_undo("Set","trace's title");
	SET_FIELD_ON_T_OR_P(title, value);
	on_update();
}

void MainWindow::on_cmbTraceX_currentTextChanged(const QString x) {
	GET_SEL_PT(false);
	push_undo("Set","trace's X");
	SET_FIELD_ON_T_OR_P(x, x);
	refresh_cmbTraceType();
	on_update();
}

void MainWindow::on_cmbTraceY_currentTextChanged(const QString y) {
	GET_SEL_PT(false);
	push_undo("Set","trace's Y");
	SET_FIELD_ON_T_OR_P(y, y);
	refresh_cmbTraceType();
	on_update();
}

void MainWindow::on_cmbTraceZ_currentTextChanged(const QString z) {
	GET_SEL_PT(false);
	push_undo("Set","trace's Z");
	SET_FIELD_ON_T_OR_P(z, z);
	refresh_cmbTraceType();
	on_update();
}

void MainWindow::on_cmbTraceType_currentTextChanged(const QString style) {
	GET_SEL_PT(false);
	push_undo("Set","trace's type");
	typedef QMap<QString,QVariant> Options;
	DO_ON_T_OR_P(
		auto options=trace->style ? trace->style->options : Options();
		if (trace->style) { delete trace->style; };
		trace->style=name_to_traceStyle(style);
		if (trace->style) { trace->style->options=options; }
		);
	on_update();
}

void MainWindow::on_btnTraceStyleOptions_clicked() {
	GET_SEL_PT(false);
	if (traces.size()==0 && plots.size()==0) { return; }

	// We pick the first style, in case we want to do multiple traces.
	// Since the options are just in a map, an unused option does not cause any troubles.
	auto style=(traces.size() ? traces.first() : plots.first()->traces.first())->style;
	if (!style) { return; }
	QStringList text;
	bool ok;

	for(auto &k:style->options.keys()) { text<<k+" = "+style->options[k].toString(); }
	const QString new_text=QInputDialog::getMultiLineText(this, "Enter options",
							      "Enter the options for "+style->name()+". Each option should come on its own line, formatted like KEY '=' VALUE.\n\n"+style->help(),
							      text.join("\n"), &ok);

	if (!ok) { return; }

	push_undo("Update","trace's style options");
	QMap<QString,QVariant> options;
	for(const QString &line : new_text.split("\n")) {
		QStringList pts=line.split("=");
		const QString key=pts[0].trimmed(); pts.removeFirst();
		const QString value=pts.join("=").trimmed();
		options[key]=str_to_qvariant(value);
		//		DEBUG(key<<"->"<<value<<"->"<<style->options[key]);
	}

	DO_ON_T_OR_P(if (trace->style) { trace->style->options=options; });

	LOG_INFO("New style: "<<style->toString());
	on_update();
}

void MainWindow::on_chkXTic_toggled() {
	GET_SEL_PT(false);
	push_undo("Toggle","x-tic");
	SET_FIELD_ON_T_OR_P(xtic, ui->chkXTic->isChecked());
	on_update(true, NoRefresh);
}

void MainWindow::on_chkYTic_toggled() {
	GET_SEL_PT(false);
	push_undo("Toggle","y-tic");
	SET_FIELD_ON_T_OR_P(ytic, ui->chkYTic->isChecked());
	on_update(true, NoRefresh);
}

void MainWindow::on_chkZTic_toggled() {
	GET_SEL_PT(false);
	push_undo("Toggle","z-tic");
	SET_FIELD_ON_T_OR_P(ztic, ui->chkZTic->isChecked());
	on_update(true, NoRefresh);
}

void MainWindow::on_btnNewWindow_clicked() {
	// WARNING: using a second window will mess up settings and stuff!
	log("Using more windows will possibly mess up settings!");
	(new MainWindow())->show();
	log("Using more windows will possibly mess up settings!");
}

void MainWindow::on_chkKeepAR_toggled(bool keepAR) {
	for(auto *img:images) {
		img->setKeepAspectRatio(keepAR);
	}
}


void MainWindow::on_cmbPlotMode_currentIndexChanged(int index) {
	set.setPlotMode(index);
	on_update(true, NoRefresh);
}

void MainWindow::on_btnResizeWindows_clicked() {
	for(auto *inst : instances) {
		if (inst==this) { continue; }

		inst->resize(this->size());
	}
}

void MainWindow::on_btnDatabase_pressed() {
	qDebug()<<"Refreshing menu";
	refresh_menuFile();
	menuFile.popup(ui->btnDatabase->pos());
}
