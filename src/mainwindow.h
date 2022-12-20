#pragma once
#include <QMainWindow>
#include <QTreeWidget>
#include <QMenu>
#include <QFileInfo>

#include "plot.h"
#include "highlighters.h"
#include "aspectratiopixmaplabel.h"
#include "columnalignedlayout.h"

#define LT_QT6 (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))

#define GET_SEL_T			QList<Trace *> traces=sel_to_trace(ui->tree, set); (void)0
#define GET_SEL_P(INC_CHILDREN)		QList<Plot *> plots=sel_to_plot(ui->tree, set, INC_CHILDREN); (void)0

#define GET_SEL_T1			Trace *trace=nullptr; { auto x=sel_to_trace(ui->tree, set); if (x.size()==1) { trace=x[0]; } } (void)0
#define GET_SEL_P1(INC_CHILDREN)	Plot *plot=nullptr; { auto x=sel_to_plot(ui->tree, set, INC_CHILDREN); if (x.size()==1) { plot=x[0]; } } (void)0

#define GET_SEL_PT(INC_CHILDREN)	GET_SEL_P(INC_CHILDREN); GET_SEL_T
#define GET_SEL_PT1(INC_CHILDREN)	GET_SEL_P1(INC_CHILDREN); GET_SEL_T1


#define QSETTINGS_GLOBAL_PATH		QDir::homePath()+"/.config/sqliteplotter.ini"
#define QSETTINGS_GLOBAL_ARGS		QSETTINGS_GLOBAL_PATH, QSettings::IniFormat
#define QSETTINGS_GLOBAL_TMP		QSettings(QSETTINGS_GLOBAL_ARGS)
#define QSETTINGS_GLOBAL            QSettings settings(QSETTINGS_GLOBAL_ARGS)

#define QSETTINGS_LOCAL_PATH(MAIN)	\
	(MAIN->root().length() ? MAIN->root() : QString("%1/.sqliteplotter.settings.%2.ini") \
    .arg(MAIN->db.dbFiles.size()==0 ? "" : QFileInfo(MAIN->db.dbFiles[0]).absolutePath()) \
    .arg(MAIN->db.dbFiles.size()==0 ? "" : QFileInfo(MAIN->db.dbFiles[0]).fileName()))
#define QSETTINGS_LOCAL_ARGS(MAIN)	QSETTINGS_LOCAL_PATH(MAIN), QSettings::IniFormat

#define QSETTINGS2(MAIN)		QSettings settings(QSETTINGS_LOCAL_ARGS(MAIN))
#define QSETTINGS2_TMP(MAIN)	QSettings(QSETTINGS_LOCAL_ARGS(MAIN))
#define QSETTINGS               QSETTINGS2(this)
#define QSETTINGS_TMP			QSETTINGS2_TMP(this)


Trace *pos_to_trace(Set &set, const QList<QVariant> &pos);
Plot *pos_to_plot(Set &set, const QList<QVariant> &pos, bool include_children);
QList<QList<QVariant>> sel_to_pos(QTreeWidget *tree);
QList<Plot *> sel_to_plot(QTreeWidget *tree, Set &set, bool include_children);
QList<Trace *> sel_to_trace(QTreeWidget *tree, Set &set);
Trace *item_to_trace(QTreeWidgetItem *item, Set &set);
Plot *item_to_plot(QTreeWidgetItem *item, Set &set, bool include_children);
QTreeWidgetItem *pos_to_item(QTreeWidget *tree, const QList<QVariant> &pos);
QTreeWidgetItem *pos_to_item(QTreeWidget *tree, const QList<int> &pos);

#define DB_FILES_SPLIT_CHAR "|"

extern const MapQStringQString EmptyOverride;
extern bool restoreDbFileAndSettingsFile;

class MyQCustomPlotter;

namespace Ui {
class MainWindow;
}

class QTableWidgetItem;
class QPlainTextEdit;
class QTextEdit;
class QLabel;

#include "plot_task.h"

// https://stackoverflow.com/a/34238144
class FocusWatcher : public QObject {
	Q_OBJECT
public:
	explicit FocusWatcher(QObject* parent = nullptr);
	virtual bool eventFilter(QObject *obj, QEvent *event) override;

signals:
	void focusChanged(bool in);
};


void log(const QString &str, const QColor &color);

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	static QVector<MainWindow *> instances;

	void restoreState();
	void storeState();

	Ui::MainWindow *ui;

	Db db;
	Set set;
    PlotOptions plotOptions;

	QString settings_file;
    void setDbFiles(const QStringList &files);
    void setSettingsFile(const QString &f);

	typedef QPair<QString,Set *> HistEntry;
	QList<HistEntry> undo_history;
	QList<HistEntry> redo_history;
	bool restoringState=false;
	void clear_history();
	void push_undo(const QString &p1, const QString &p2);
	void undo();
	void redo();
	void post_xdo(const bool keep_sel);
	void updateHistoryButtons();

    void setCustomWindowTitle(const QString &extra, const QString &table);
    QString root() const { return settings_file; }
	void refresh_tree(bool keep_sel);

	void log(const QString &str, const QColor &color=Qt::black);

	// Keeps track of unique values for variables
	QMap<QString,QStringList> unique_values;

	// Keeping track of total rows is sometimes useful to estimate how much has
	// changed between reloads.
	unsigned n_total_rows=0;

	void add_used_value(const QString &var, const QString &value);
	QMap<QString,QStringList> used_values;

	QMenu menuFile, menuSettings, menuCheck, menuExport;

    MySqliteHighlighter *hlPlotDefaultQuery, *hlTraceQuery, *hlFinalQuery;

	// Currently selected entries. Used to determine if we should replot, when selection changes.
	Plot *sel_plot=nullptr;
    QList<AspectRatioPixmapLabel *> images;
    QList<MyQCustomPlotter *> qcustomPlotImages;

	// refresh_tree= {0: disable, 1: refresh_tree(false), 2: refresh_tree(true)}
	enum RefreshTreeMode {
		NoRefresh,
		RefreshTreeNoKeepSel,
		RefreshTreeKeepSel,
	};
	void on_update(bool store = true, RefreshTreeMode refresh = RefreshTreeKeepSel, bool autoReplotIfOk = true);
	void on_db_reload();
    void loadFiles(const QStringList &files);
    void loadAdditionalFiles(const QStringList &files);
    void loadFile(const QFileInfo &fi);
    void setSelectedTrace(int traceIdx, bool blockSignals);

    void setCommandLineOverrides(const QMap<QString,QString> &overrides);
private:
	void refresh_menuFile();
	void refresh_menuSettings();

	void refresh_lstGlobalVars();
	void refresh_lstPlotVars();
	void refresh_lstTraceVars();
	QString tblGlobalVars_old_col_name, tblPlotVars_old_col_name, tblTraceVars_old_col_name;
	int logCounter=0;

	void refresh_cmbTraceType();

	int storeFirstIndex=0; // This is the first index of images in which we can write new images.
	// All previous ones should not be touched.

	ColumnAlignedLayout *tblDataHeadersLayout;
	ColumnAlignedLayout *tblDataHeadersAvgLayout;
	QVector<QLineEdit *> tblDataColFilters;
	QVector<QLabel *> tblDataColAvg;

	bool initializing;

	void _editingFinished(QTextEdit &);
	template<class T=QWidget>
    T *getWidgetForPlotMode(const int index);
    void refresh_cmbTraceColumns(bool blockSignals);
private slots:
	void on_btnUndo_clicked() { undo(); }
	void on_btnRedo_clicked() { redo(); }

	void on_lblUniqueValues_linkActivated(const QString &link);

	void on_btnAddGlobalVar_clicked();
	void on_btnDeleteGlobalVar_clicked();
	void on_tblGlobalVars_itemChanged(QTableWidgetItem *item);
	void on_tblGlobalVars_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *);
	void _on_btnSaveImages_clicked();
	void handleVarsContextMenu(const QPoint& pos);

	void on_btnPlot_clicked();
	void on_btnStoreAdd_clicked();
	void on_btnStoreRemove_clicked();
	void on_btnClearStore_clicked();
	void on_chkAutoReplot_toggled();
	void on_chkLegend_toggled();
	void on_chkTitle_toggled();

	void on_btnNewPlot_clicked();
	void on_btnNewTrace_clicked();
	void on_btnClone_clicked();
	void on_btnMulticlone_clicked();
	void on_btnMulticlone2_clicked();
	void on_btnDelete_clicked();
	void on_btnMoveUp_clicked();
	void on_btnMoveDown_clicked();

	void on_txtPlotDefaultQuery_textChanged();
	void _on_txtPlotDefaultQuery_editingFinished();
	void on_txtPlotTitle_editingFinished();
	void on_txtXAxisLabel_editingFinished();
	void on_txtYAxisLabel_editingFinished();
	void on_txtZAxisLabel_editingFinished();
	void on_chkLogX_toggled();
	void on_chkLogY_toggled();
	void on_chkLogZ_toggled();
	void on_txtYMin_editingFinished();
	void on_txtYMax_editingFinished();
	void on_txtXMin_editingFinished();
	void on_txtXMax_editingFinished();
	void on_txtZMin_editingFinished();
	void on_txtZMax_editingFinished();
	void on_txtAltColumns_valueChanged(int arg1);
	void on_txtAltRows_valueChanged(int arg1);
	void on_btnAddPlotVar_clicked();
	void on_btnDeletePlotVar_clicked();
	void on_tblPlotVars_itemChanged(QTableWidgetItem *item);
	void on_tblPlotVars_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *);
	void on_chkStableColors_toggled(bool);
	void on_chkBBox_toggled(bool);
	void on_chkD3Grid_toggled(bool);
	void on_txtD3GridMesh_editingFinished();
	void on_txtD3GridMesh_valueChanged(int);
	void on_txtXrot_editingFinished();
	void on_txtXrot_valueChanged(int);
	void on_txtZrot_editingFinished();
	void on_txtZrot_valueChanged(int);

	void on_tabs_currentChanged(int index);

	void on_txtTraceQuery_textChanged();
	void _on_txtTraceQuery_editingFinished();
	void on_txtTraceTitle_editingFinished();
	void on_cmbTraceX_currentTextChanged(const QString);
	void on_cmbTraceY_currentTextChanged(const QString);
	void on_cmbTraceZ_currentTextChanged(const QString);
	void on_cmbTraceType_currentTextChanged(const QString);
	void on_btnAddTraceVar_clicked();
	void on_btnDeleteTraceVar_clicked();
	void on_tblTraceVars_itemChanged(QTableWidgetItem *item);
	void on_tblTraceVars_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *);
	void on_btnTraceStyleOptions_clicked();

	void on_btnClearLog_clicked();

	void on_chkXTic_toggled();
	void on_chkYTic_toggled();
	void on_chkZTic_toggled();

	void updateTableAverages();
	void on_btnQueryData_clicked();
	void on_btnExportData_clicked();
	void on_txtQueryColumnNameFilter_textChanged(const QString &);
	void on_txtQueryColumnNameFilter_editingFinished();
	void invalidateAlignedLayout();
	void filterTableRows();

	void on_tree_itemChanged(QTreeWidgetItem * item, int column);
	void on_tree_itemSelectionChanged();
	void on_btnNewWindow_clicked();
	void on_chkKeepAR_toggled(bool checked);
	void on_cmbPlotMode_currentIndexChanged(int index);
	void on_btnResizeWindows_clicked();
	void on_btnSortTraces_clicked();
	void _on_btnCheckAll_clicked();
	void _on_btnUncheckAll_clicked();
	void _on_btnCheckWithData_clicked();
	void _on_btnSaveData_clicked();
	void on_btnDatabase_pressed();
    void on_btnZoomToRect_clicked();
};
