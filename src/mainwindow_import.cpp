#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "misc.h"

#include <QDir>

void MainWindow::loadFile(const QFileInfo &fi) {
    loadFiles(QStringList()<<fi.absoluteFilePath());
}

void MainWindow::loadAdditionalFiles(const QStringList &files) {
    LOG_INFO("MainWindow::loadFile "<<files.join(DB_FILES_SPLIT_CHAR));
	QStringList allFiles=db.dbFiles;
	for(const QString &f:files) {
		if (allFiles.contains(f)==false) {
			allFiles<<f;
		}
	}
    clear_history();
    storeState();
    setDbFiles(allFiles);
    refresh_menuFile();
}

void MainWindow::loadFiles(const QStringList &files) {
	db.dbFiles.clear();
	loadAdditionalFiles(files);
}

void MainWindow::setDbFiles(const QStringList &files) {
    {
        db.dbFiles=files;
        QSETTINGS_GLOBAL;
        QStringList history=settings.value("history").toStringList();
        history.removeAll(files.join(DB_FILES_SPLIT_CHAR));
        history.insert(0, files.join(DB_FILES_SPLIT_CHAR));
        settings.setValue("history", history);
    }
    restoreState();
}
