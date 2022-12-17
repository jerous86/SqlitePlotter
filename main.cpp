#include "mainwindow.h"
#include <stdlib.h>
#include <QApplication>
#include <QStringList>


QString full_binary_path(const QString &binary);

int main(int argc, char *args[])
{
    QApplication a(argc, args);

//#ifdef Q_OS_WINDOWS
//#else
//    setenv("PATH", (QString::fromStdString(std::string(getenv("PATH")))+"/usr/local/bin").toStdString().c_str(), true);
//    setenv("GNUPLOT_IOSTREAM_CMD", (full_binary_path("gnuplot")+" -persist").toStdString().c_str(), true);
//#endif
    init_importers();

    QStringList dbFiles;
    for(int i=1; i<argc; i++) {
        if (QFileInfo(QString(args[i])).exists()) {
            dbFiles<<args[i];
        }
    }

	if (dbFiles.size()>0) {
		// Avoids loading the files on the command line twice.
		restoreDbFileAndSettingsFile=false;
	}
    MainWindow w;
	restoreDbFileAndSettingsFile=true;

    if (dbFiles.size()>0) {
        qDebug()<<"Loading "<<dbFiles.size()<<" files, passed from the command line";
		w.loadFiles(dbFiles);
	}
    w.show();

	return a.exec();
}
