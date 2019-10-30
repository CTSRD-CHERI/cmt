#!/bin/sh -e

#
# Input: as produced by lifetimes.py.
#
# Output: three columns:
#
# 0. Number of occurences of each (lifetime, size) pair
# 1. Allocation life time, from allocation to free
# 2. Allocation size
#

if [ $# -ne 2 ]; then
	echo "usage: $0 input-file output-file" 1>&2
	exit 1
fi

grep -v malloc "$1" | sed 's/.* # //' | awk '{ print $4, $1 }' | sort -bg -k 1,2 | uniq -c > "$2"

