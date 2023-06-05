#!/bin/sh
# Reads IP packets from a pcap file, and outputs for each packet possible valuable information.

set -ue

FILEPATH="$1"
FILENAME="$(basename "$FILEPATH")"

printf "TIME\tFRAME_LEN\tIP_PROTO\tIP_SRC\tIP_DST\tTCP_SRC_PORT\tTCP_DST_PORT\tTCP_WINDOW_SIZE\tTCP_ACK\tUDP_SRC_PORT\tUDP_DST_PORT\tFILENAME\n"
tshark -r "$FILEPATH" -T fields -E separator="	" \
	-e frame.time_relative \
	-e frame.len \
	-e ip.proto \
	-e ip.src \
	-e ip.dst \
	-e tcp.srcport \
	-e tcp.dstport \
	-e tcp.window_size \
	-e tcp.flags.ack \
	-e udp.srcport \
	-e udp.dstport \
	| sed "s#\$#\t${FILENAME}#g"

