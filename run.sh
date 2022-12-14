#!/usr/bin/env bash

set -uex

ROOT=$(cd "$(dirname "$0")"; echo $PWD)
$ROOT/build.sh SqlitePlotter importers
$ROOT/bin/SqlitePlotter "${@}"
