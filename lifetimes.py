#!/usr/bin/env python3

#
# Turns output from "trace2cmt -s", eg:
#
# malloc(8) = 81d220b48<6655>
# realloc(81d220b48<6655>, 16) = 81d3b83a0<6656>
# free(81d3b83a0<6656>)
#
# ... into this:
#
# malloc(8) = 81d220b48<6655>
# realloc(81d220b48<6655>, 16) = 81d3b83a0<6656>    # 8 bytes, allocated 1 lines ago
# free(81d3b83a0<6656>)                             # 16 bytes, allocated 1 lines ago
#

import re
import sys

allocated = {}
sizes = {}

def parse_ptr(l):
	return int(re.split("[<>]+", l)[0], 16)

def do_line(l, lineno):
	f = re.split("[()= ]+", l)
	if f[0] == "malloc":
		size = f[1]
		ptr = parse_ptr(f[2])
		sizes[ptr] = size
		allocated[ptr] = lineno

		#return "ptr {}, size {}, lineno {}".format(ptr, size, lineno)
		return None

	if f[0] == "free":
		ptr = parse_ptr(f[1])

		#return "allocated at {}, {} lines ago".format(allocated[ptr], lineno - allocated[ptr])
		return "{} bytes, allocated {} lines ago".format(sizes[ptr], lineno - allocated[ptr])

	if f[0] == "realloc":
		size = f[2]
		oldptr = parse_ptr(f[1])
		newptr = parse_ptr(f[3])
		oldsize = sizes[oldptr]
		oldallocated = lineno - allocated[oldptr]

		sizes[newptr] = size
		allocated[newptr] = lineno

		#return "allocated at {}, {} lines ago".format(allocated[oldptr], lineno - allocated[oldptr])
		return "{} bytes, allocated {} lines ago".format(oldsize, oldallocated)

	sys.exit("cannot parse line: \"{}\"".format(l))

if len(sys.argv) != 2:
	sys.exit("usage: {} input-file".format(sys.argv[0]))

lineno = 0
f = open(sys.argv[1],'r')
for l in f:
	lineno = lineno + 1
	l = l.rstrip()
	out = do_line(l, lineno)
	if out:
		print("{:49} # {}".format(l, out))
	else:
		print(l)
