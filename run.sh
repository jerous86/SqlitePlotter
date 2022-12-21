#!/usr/bin/env bash

set -uex

export QT_LOGGING_RULES="*.debug=true;qt.qpa.input*.debug=false;qt.widgets.gestures.debug=false"
ROOT=$(cd "$(dirname "$0")"; echo $PWD)
$ROOT/build.sh SqlitePlotter importers
$ROOT/bin/SqlitePlotter "${@}"
