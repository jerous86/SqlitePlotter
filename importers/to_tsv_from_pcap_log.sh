#!/bin/sh

keys=$(echo $(basename "$1") | kvs --kvpattern '([a-zA-Z]+)=([^_]+)' --kvfmt %k --replace-key limit rlimit)
values=$(echo $(basename "$1") | kvs --kvpattern '([a-zA-Z]+)=([^_]+)' --kvfmt '"%v"' --field-sep ',')

# CWND is in octets
printf "TIME\tCWND\tRTT\tRTTVAR\tFILENAME\t${keys}\n"
grep -r iperf.*cwnd "$1" | cut -f2,5,9,11 | awk -v OFS='\t' "{ F++; if (F==1) T0=\$1; print \$1/1e6, \$2*8, \$3/1e6, \$4/1e6, \"$(basename "$1")\", $values }"

#awk '/iperf_print_tcp_info/ { F++; if (F==1) T0=$2; print ($2-T0)/1e6,$5}'
#grep -r iperf.*cwnd "$1" | cut -f2,5,9,11 | tf t-apply '(t-t0)' | awk -v OFS='\t' "{print \$1/1e6, \$2*8, \$3/1e6, \$4/1e6, \"$(basename "$1")\", $values }"
