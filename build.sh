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

if [ $# -eq 0 ]; then $0 SqlitePlotter importers; exit 0; fi

while [ $# -gt 0 ]; do
	case $1 in
		SqlitePlotter)
			build_SqlitePlotter
			;;
		importers) 
			make -C importers all
			;;
		*) echo "Don't know how to build '$1'. Ignoring." ;;
	esac
	shift
done

cp $(ls importers/*.cpp | sed s'#[.]cpp##g') $BIN_DIR
cp to_tsv_from_*.sh $BIN_DIR/
cp sqliteplotter*.ini $BIN_DIR/
