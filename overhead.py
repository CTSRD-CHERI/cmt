#!/usr/bin/env python3

#
# Turns a trace, or the output from "trace2cmt -s", eg:
#
# malloc(17) = 81d3bd6c0<6385>
# malloc(7) = 81d220b40<6386>
# malloc(18) = 81d3bd6e0<6387>
# malloc(12) = 81d3b8590<6388>
# malloc(27) = 81d3bd700<6389>
# malloc(256) = 81d360600<6390>
# realloc(81d360600<6390>, 512) = 81d3b9800<6391>
# realloc(81d3b9800<6391>, 1024) = 81d3bbc00<6392>
# free(81d220b28<6306>)
# free(81d3b83b0<6307>)
# free(81d3b83c0<6308>)
# free(81d3b83d0<6309>)
# free(81d3bd100<6310>)
# free(81d3bd120<6311>)
#
# ... into this:
#
# malloc(17) = 81d3bd6c0<6385>                      # prev 81d3bd6a0, gap 12, next 81de0d000, gap 10811695
# malloc(7) = 81d220b40<6386>                       # prev 81d220b38, gap 0, next 81d221700, gap 3001
# malloc(18) = 81d3bd6e0<6387>                      # prev 81d3bd6c0, gap 15, next 81de0d000, gap 10811662
# malloc(12) = 81d3b8590<6388>                      # prev 81d3b8580, gap 0, next 81d3ba200, gap 7268
# malloc(27) = 81d3bd700<6389>                      # prev 81d3bd6e0, gap 14, next 81de0d000, gap 10811621
# malloc(256) = 81d360600<6390>                     # prev 81d360300, gap 512, next 81d363800, gap 12544
# realloc(81d360600<6390>, 512) = 81d3b9800<6391>   # 256 bytes, allocated 1 lines ago, prev 81d360300, gap 512, next 81d363800, gap 12544
# realloc(81d3b9800<6391>, 1024) = 81d3bbc00<6392>  # 512 bytes, allocated 1 lines ago, prev 81d3b8590, gap 4708, next 81d3ba200, gap 2048
# free(81d220b28<6306>)                             # 6 bytes, allocated 87 lines ago, prev 81d220b20, gap 1, next 81d220b30, gap 2
# free(81d3b83b0<6307>)                             # 9 bytes, allocated 87 lines ago, prev 81d3b83a0, gap 4, next 81d3b83c0, gap 7
# free(81d3b83c0<6308>)                             # 10 bytes, allocated 87 lines ago, prev 81d3b83a0, gap 20, next 81d3b83d0, gap 6
# free(81d3b83d0<6309>)                             # 16 bytes, allocated 87 lines ago, prev 81d3b83a0, gap 36, next 81d3b83e0, gap 0
# free(81d3bd100<6310>)                             # 18 bytes, allocated 87 lines ago, prev 81d3bd0e0, gap 0, next 81d3bd120, gap 14
# free(81d3bd120<6311>)                             # 17 bytes, allocated 87 lines ago, prev 81d3bd0e0, gap 32, next 81d3bd140, gap 15
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

def atop(ptr):
	return ptr >> 12

def ptoa(page):
	return page << 12

def neighbours(ptr):
	index = sizes.index(ptr)
	size = sizes[ptr]

	if index > 0:
		try:
			prev_pointer, prev_size = sizes.peekitem(index - 1)
			assert prev_pointer < ptr
			assert prev_pointer + prev_size <= ptr
		except IndexError:
			prev_pointer = None
			prev_size = None
	else:
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
	assert ptr > 0

	size = sizes[ptr]
	age = lineno - allocated[ptr]

	first_page = atop(ptr)
	last_page = atop(ptr + size - 1)

	prev_pointer, prev_size, next_pointer, next_size = neighbours(ptr)

	if prev_pointer:
		prev_gap = ptr - prev_pointer - prev_size
	else:
		prev_pointer = 0
		prev_gap = -1

	if next_pointer:
		next_gap = next_pointer - ptr - size
	else:
		next_pointer = 0
		next_gap = -1

	del allocated[ptr]
	del sizes[ptr]

	return "{} bytes, allocated {} lines ago, prev {:x}, gap {}, next {:x}, gap {}, pages [{}-{}]".format(size, age, prev_pointer, prev_gap, next_pointer, next_gap, first_page, last_page)

def do_malloc(ptr, size, lineno):
	assert ptr > 0
	assert size > 0
	assert ptr not in sizes
	assert ptr not in allocated

	sizes[ptr] = size
	allocated[ptr] = lineno

	first_page = atop(ptr)
	last_page = atop(ptr + size - 1)
	prev_pointer, prev_size, next_pointer, next_size = neighbours(ptr)

	more_pages = last_page - first_page + 1

	if prev_pointer:
		prev_gap = ptr - prev_pointer - prev_size
		prev_page = atop(prev_pointer + prev_size - 1)
		assert ptoa(prev_page) < ptr
		if prev_page == first_page:
			more_pages = more_pages - 1
	else:
		prev_pointer = 0
		prev_gap = -1
		prev_page = -1

	if next_pointer:
		next_gap = next_pointer - ptr - size
		next_page = atop(next_pointer)
		if first_page != last_page and last_page == next_page:
			more_pages = more_pages - 1
	else:
		next_pointer = 0
		next_gap = -1
		next_page = -1

	assert more_pages >= 0
	#return "prev {:x}, gap {}, last page {}, next {:x}, gap {}, first page {}, pages [{}, {}], {} more pages ".format(prev_pointer, prev_gap, prev_page, next_pointer, next_gap, next_page, first_page, last_page, more_pages)
	return "prev {:x}, gap {}, next {:x}, gap {}, pages [{}, {}], {} more pages ".format(prev_pointer, prev_gap, next_pointer, next_gap, first_page, last_page, more_pages)

def do_line(l, lineno):
	f = re.split("[()=, ]+", l)
	if f[0] == "malloc":
		size = int(f[1])
		ptr = parse_ptr(f[2])

		return do_malloc(ptr, size, lineno)

	if f[0] == "free":
		oldptr = parse_ptr(f[1])

		return do_free(oldptr, lineno)

	if f[0] == "realloc":
		oldptr = parse_ptr(f[1])
		newsize = int(f[2])
		newptr = parse_ptr(f[3])

		tmp = do_free(oldptr, lineno)
		do_malloc(newptr, newsize, lineno)

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
