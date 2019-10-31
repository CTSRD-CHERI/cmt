#!/bin/sh -e

#
# Input: output from kdump(1).  Use it like this:
#
# export MALLOC_CONF=utrace:true
# ktrace -t +u binary-to-run
# kdump | ./kdump2trace.sh
#
# Output: trace, as required for trace2cmt.
#

awk -F '[\t (),]+' '
	$4 == "USER" && $5 == "free" {
		if ($6 != 0)
			print "free(" $6 ")"
	}
	$4 == "USER" && $7 == "malloc" {
		print "malloc(" $8 ") = " $5
	}
	$4 == "USER" && $7 == "realloc" {
		print "realloc(" $8 ", " $9 ") = " $5
	}'

