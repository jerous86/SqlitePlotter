#include "mainwindow.h"
#include <stdlib.h>
#include <QApplication>
#include <QStringList>
#include <QMap>


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
    QMap<QString,QString> overrides;
    for(int i=1; i<argc; i++) {
        if (QFileInfo(QString(args[i])).exists()) {
            dbFiles<<args[i];
        } else if (QString(args[i]) == "--var") {
            overrides[args[i+1]]=args[i+2];
            i+=2;
        } else if (QString(args[i]) == "--global-var") {
            overrides[QString("global:%1").arg(args[i+1])]=args[i+2];
            i+=2;
        } else if (QString(args[i]) == "--plot-var") {
            overrides[QString("plot[%1]:%2").arg(args[i+1],args[i+2])]=args[i+3];
            i+=3;
        } else if (QString(args[i]) == "--trace-var") {
            overrides[QString("trace[%1][%2]:%3").arg(args[i+1],args[i+2],args[i+3])]=args[i+4];
            i+=4;
        } else {
            qWarning()<<"Unhandled CLI argument '"<<args[i]<<"' at position "<<i;
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
    w.setCommandLineOverrides(overrides);
    w.show();

	return a.exec();
}
