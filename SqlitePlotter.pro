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

INCLUDEPATH += src/ src/sqlite-amalgamation-3390300

SOURCES +=  \
    src/main.cpp  \
    src/mainwindow.cpp \
    src/columnalignedlayout.cpp \
    src/mainwindow_data.cpp \
    src/mainwindow_import.cpp \
    src/mainwindow_plot.cpp \
    src/mainwindow_tree.cpp \
    src/mainwindow_undo.cpp \
    src/mainwindow_vars.cpp \
    src/myplotter_helpers.cpp \
    src/plot.cpp \
    src/db.cpp \
    src/plot_myplotter.cpp \
    src/plot_myqcustomplotter.cpp \
    src/qcustomplot.cpp \
    src/plot_task.cpp \
    src/libsqlitefunctions.c \
    src/percentile.c \
    src/tracestyle.cpp \
    src/batchsave.cpp \
    src/highlighters.cpp \
    src/aspectratiopixmaplabel.cpp \
    src/misc.cpp \
    src/tracestyle/StackedLine.cpp \
    src/tracestyle/VertHist.cpp \
    src/tracestyle/histogramxd.cpp \
    src/tracestyle/pointsxd.cpp \
    src/tracestyle/violinplot.cpp \
    src/sqlite-amalgamation-3390300/sqlite3.c

HEADERS += \
    src/mainwindow.h \
    src/columnalignedlayout.h \
    src/my_math.h \
    src/myplotter_helpers.h \
    src/myplotter_scene.h \
    src/plot.h \
    src/db.h \
    src/misc.h \
    src/plot_myplotter.h \
    src/plot_myqcustomplotter.h \
    src/qcustomplot.h \
    src/plot_task.h \
    src/tracestyle/helpers.h \
    src/tracestyles.h \
    src/batchsave.h \
    src/highlighters.h \
    src/aspectratiopixmaplabel.h \
    src/sqlite-amalgamation-3390300/sqlite3.h

FORMS +=  src/mainwindow.ui  src/batchsave.ui
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
