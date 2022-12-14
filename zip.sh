#!/bin/sh

set -uex

ZIP_FILE=SqlitePlotter.zip

cd "$(dirname "$0")"
rm -rf $ZIP_FILE

./build.sh SqlitePlotter importers

(cd bin; zip ../$ZIP_FILE ./*)
