#!/bin/sh
# Converts a pcap file to a series of [START,START+INTERVAL, FRAME_LEN] stamps.
set -ue

FILE_PATH="$1"
FILE_NAME="$(basename "$FILE_PATH")"

# Sums packets in interval of ${INTERVAL} seconds. This might be useful to reduce the amount of rows.
# The default is grouping by 0.001 seconds.
INTERVAL="${2:-0.001}"

printf "TIME\tTIME2\tFRAME_LEN\tFILE_NAME\n"
tshark -q -r "$FILE_PATH" -z "io,stat,${INTERVAL},SUM(frame.len)frame.len" \
	| grep '<>' \
	| tr -d '|<>' \
	| awk "{print \$1, \"\t\", \$2, \"\t\", \$3, \"\t${FILE_NAME}\"}"

