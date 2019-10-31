#!/bin/sh -e

#
# Input: output from kdump(1), which looks like this:
#
# 61039 make     USER  0x800607420 = malloc(26)
# 61039 make     USER  0x800607440 = realloc(0x800610058, 17)
# 61039 make     USER  free(0x0)
# 61039 make     USER  free(0x0)
# 61039 make     USER  0x800607460 = malloc(32)
# 61039 make     USER  0x800610058 = malloc(2)
# 61039 make     USER  0x800615720 = malloc(35)
# 61039 make     USER  free(0x800618480)
#
# Use it like this:
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

