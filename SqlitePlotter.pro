#-------------------------------------------------
#
# Project created by QtCreator 2018-05-29T16:31:25
#
#-------------------------------------------------

QT += core gui widgets printsupport
QT += qml # for using js to calculate math :( qtdeclarative5-dev
#CONFIG += console
CONFIG += warn_on

TARGET = SqlitePlotter
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
DEFINES += QT_FORCE_ASSERTS

win32 {
} else {
    LIBS += -L/usr/local/lib/
    LIBS += -L/usr/local/lib/ -lboost_iostreams -lboost_system -lboost_filesystem
	LIBS += -ldl # necessary for sqlite3 on WSL
    INCLUDEPATH+=/usr/local/opt/boost/include/
}

;CONFIG += release
;CONFIG += debug
;QMAKE_CXXFLAGS += -g

INCLUDEPATH += sqlite-amalgamation-3390300

SOURCES +=  main.cpp  \
mainwindow.cpp \
    columnalignedlayout.cpp \
    mainwindow_data.cpp \
    mainwindow_import.cpp \
    mainwindow_plot.cpp \
    mainwindow_tree.cpp \
    mainwindow_undo.cpp \
    mainwindow_vars.cpp \
    myplotter_helpers.cpp \
    plot.cpp \
    db.cpp \
    plot_myplotter.cpp \
    plot_myqcustomplotter.cpp \
    qcustomplot.cpp \
    plot_task.cpp \
    libsqlitefunctions.c percentile.c \
    tracestyle.cpp \
    batchsave.cpp \
    highlighters.cpp \
    aspectratiopixmaplabel.cpp \
    misc.cpp \
    tracestyle/StackedLine.cpp \
    tracestyle/VertHist.cpp \
    tracestyle/histogramxd.cpp \
    tracestyle/pointsxd.cpp \
    tracestyle/violinplot.cpp \
    sqlite-amalgamation-3390300/sqlite3.c

HEADERS +=  mainwindow.h \
    columnalignedlayout.h \
    my_math.h \
    myplotter_helpers.h \
    myplotter_scene.h \
    plot.h \
    db.h \
    misc.h \
    plot_myplotter.h \
    plot_myqcustomplotter.h \
    qcustomplot.h \
    plot_task.h \
    tracestyle/helpers.h \
    tracestyles.h \
    batchsave.h \
    highlighters.h \
    aspectratiopixmaplabel.h \
    sqlite-amalgamation-3390300/sqlite3.h

FORMS +=  mainwindow.ui  batchsave.ui
ICON = icon.icns

# If encountering
#dyld: Symbol not found: __cg_jpeg_resync_to_restart
#  Referenced from: /System/Library/Frameworks/ImageIO.framework/Versions/A/ImageIO
#  Expected in: /usr/local/lib//libJPEG.dylib
# in /System/Library/Frameworks/ImageIO.framework/Versions/A/ImageIO
#01:48:56: The program has unexpectedly finished.
#
# unset DYLD_LIBRARY in projects/run environment
# Source: https://stackoverflow.com/questions/38131011/qt-application-throws-dyld-symbol-not-found-cg-jpeg-resync-to-restart
