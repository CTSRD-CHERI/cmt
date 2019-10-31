#!/bin/sh -e

#
# Input: output from kdump(1), which looks like this:
#
# call-trace	1536334807907404599	50a0805 14	malloc	96	81d21e100
# call-trace	1536334807907411147		free	81d2899a0	
# call-trace	1536334807907413594	50ad65a 13	malloc	16	81d239670
# call-trace	1536334807909430574	8099e0663 16	realloc	81d220288 340	81d279480
# call-trace	1536334807909463803	8099dfc02 18	malloc	96	81d21ebe0
#
# Output: trace, as required for trace2cmt.
#

awk '
	$1 == "call-trace" && $3 == "free" {
		print "free(" $4 ")"
	}
	$1 == "call-trace" && $5 == "malloc" {
		print "malloc(" $6 ") = " $7
	}
	$1 == "call-trace" && $5 == "realloc" {
		print "realloc(" $6 ", " $7 ") = " $8
	}'
