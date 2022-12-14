#!/usr/bin/env bash

set -ue

cd "$(dirname "$0")"

ROOT="$PWD"
BUILD_DIR="$ROOT/qbuild"
BIN_DIR="$ROOT/bin"

mkdir -p $BUILD_DIR
mkdir -p $BIN_DIR/


build_SqlitePlotter() (
	echo "Building SqlitePlotter"
	cd $BUILD_DIR
	
	if which qmake >/dev/null; then
		qmake CONFIG+=release ../SqlitePlotter.pro
	elif which qmake-qt6 >/dev/null; then
		qmake-qt6 CONFIG+=release ../SqlitePlotter.pro
	elif which qmake-qt5 >/dev/null; then
		qmake-qt5 CONFIG+=release ../SqlitePlotter.pro
	fi
	
	make -j8
	
	case $OSTYPE in
		darwin*) cp $BUILD_DIR/SqlitePlotter.app/Contents/MacOs/SqlitePlotter $BIN_DIR/ ;;
		*) cp $BUILD_DIR/SqlitePlotter $BIN_DIR/ ;;
	esac
	echo "    Done"
)

build_importers() (
	echo "Building importers"
	case $OSTYPE in
		darwin*)
			CXXFLAGS="-pipe -stdlib=libc++ -O2 -std=gnu++1z -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -mmacosx-version-min=12.0 -Wall -Wextra -fPIC"
			INCPATH="-I /usr/local/include/QtCore/ -F/usr/local/lib"
			LIBS="-F/usr/local/lib -L/usr/local/lib/ -framework QtCore"
			;;
		*)
			CXXFLAGS="-O2 -Wall -Wextra --std=c++11 -fPIC"
			INCPATH="$(pkg-config --cflags Qt5Core)"
			LIBS="$(pkg-config --libs Qt5Core)"
			;;
	esac

	mkdir -p $BIN_DIR/
	for importer in to_tsv_*.cpp; do
		OUT=$BIN_DIR/$(basename $importer .cpp)
		echo "    $importer -> $OUT"
		clang++ $INCPATH $CXXFLAGS $LIBS $importer -o "$OUT"
	done
	cp sqliteplotter-importers*.ini $BIN_DIR/
	echo "    Done"
)

if [ $# -eq 0 ]; then $0 SqlitePlotter importers; fi

while [ $# -gt 0 ]; do
	case $1 in
		SqlitePlotter) build_SqlitePlotter ;;
		importers) build_importers ;;
		*) echo "Don't know how to build '$1'. Ignoring." ;;
	esac
	shift
done
