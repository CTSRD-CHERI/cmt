#!/usr/bin/env python3

#
# Turns a trace, or the output from "trace2cmt -s", eg:
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
from sortedcontainers import SortedDict

# Line number of the malloc(), indexed by pointer.
allocated = SortedDict()

# Size of the malloc(), indexed by pointer.
sizes = SortedDict()

def parse_ptr(l):
	return int(re.split("[<>]+", l)[0], 16)

def do_free(ptr, lineno):
	size = sizes[ptr]
	age = lineno - allocated[ptr]

	index = sizes.index(ptr)

	try:
		prev_pointer, _ = sizes.peekitem(index - 1)
	except IndexError:
		prev_pointer = 0

	try:
		next_pointer, _ = sizes.peekitem(index + 1)
	except IndexError:
		next_pointer = 0

	del allocated[ptr]
	del sizes[ptr]

	return "{} bytes, allocated {} lines ago, prev {:x}, next {:x}".format(size, age, prev_pointer, next_pointer)

def do_line(l, lineno):
	f = re.split("[()=, ]+", l)
	if f[0] == "malloc":
		size = f[1]
		ptr = parse_ptr(f[2])

		sizes[ptr] = size
		allocated[ptr] = lineno

		return None

	if f[0] == "free":
		oldptr = parse_ptr(f[1])

		return do_free(oldptr, lineno)

	if f[0] == "realloc":
		oldptr = parse_ptr(f[1])
		size = f[2]
		newptr = parse_ptr(f[3])

		tmp = do_free(oldptr, lineno)

		sizes[newptr] = size
		allocated[newptr] = lineno

		return tmp

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
