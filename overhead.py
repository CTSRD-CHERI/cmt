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

def neighbours(ptr):
	index = sizes.index(ptr)
	size = sizes[ptr]

	try:
		prev_pointer, prev_size = sizes.peekitem(index - 1)
		#print("prev {:x}, prev_size {}, end {:x}, ptr {:x}, gap {}".format(prev_pointer, prev_size, prev_pointer + prev_size, ptr, ptr - prev_pointer + prev_size))
		#assert prev_pointer < ptr
		if prev_pointer >= ptr:
			# wtf
			prev_pointer = 0
			prev_size = 0
		else:
			assert prev_pointer + prev_size <= ptr
	except IndexError:
		prev_pointer = None
		prev_size = None

	try:
		next_pointer, next_size = sizes.peekitem(index + 1)
		assert next_pointer > ptr
		assert ptr + size <= next_pointer
	except IndexError:
		next_pointer = None
		next_size = None

	return prev_pointer, prev_size, next_pointer, next_size

def do_free(ptr, lineno):
	size = sizes[ptr]
	age = lineno - allocated[ptr]

	index = sizes.index(ptr)

	prev_pointer, prev_size, next_pointer, next_size = neighbours(ptr)

	if prev_pointer:
		prev_gap = ptr - prev_pointer + prev_size
	else:
		prev_pointer = 0
		prev_gap = -1

	if next_pointer:
		next_gap = next_pointer - ptr + size
	else:
		next_pointer = 0
		next_gap = -1

	del allocated[ptr]
	del sizes[ptr]

	return "{} bytes, allocated {} lines ago, prev {:x}, gap {}, next {:x}, gap {}".format(size, age, prev_pointer, prev_gap, next_pointer, next_gap)

def do_line(l, lineno):
	f = re.split("[()=, ]+", l)
	if f[0] == "malloc":
		size = int(f[1])
		ptr = parse_ptr(f[2])

		assert ptr not in sizes
		assert ptr not in allocated
		sizes[ptr] = size
		allocated[ptr] = lineno

		return None

	if f[0] == "free":
		oldptr = parse_ptr(f[1])

		return do_free(oldptr, lineno)

	if f[0] == "realloc":
		oldptr = parse_ptr(f[1])
		size = int(f[2])
		newptr = parse_ptr(f[3])

		tmp = do_free(oldptr, lineno)

		assert newptr not in sizes
		assert newptr not in allocated
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
