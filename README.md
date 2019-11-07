# Compact Memory Traces

This repo contains utilities for analyzing the behaviour of memory allocators.

First, you need the traces.  Either convert them from dtrace ones
(https://github.com/CTSRD-CHERI/memory-alloc-tracing,
https://github.com/CTSRD-CHERI/memory-alloc-tracing/wiki/Trace-directory)
using `log2trace.sh`, or generate new ones.  On FreeBSD with jemalloc, use:
```
$ export MALLOC_CONF=utrace:true
$ ktrace -t u your-binary
$ kdump | ./kdump2trace.sh > your-binary.trace
```

Traces look roughly like this:
```
malloc(8) = 81d220b48<6655>
realloc(81d220b48<6655>, 16) = 81d3b83a0<6656>
free(81d3b83a0<6656>)
```

The `lifetimes.py` utility will annotate traces with allocation lifetime information, eg:
```
malloc(8) = 81d220b48<6655>
realloc(81d220b48<6655>, 16) = 81d3b83a0<6656>    # 8 bytes, allocated 1 lines ago
free(81d3b83a0<6656>)                             # 16 bytes, allocated 1 lines ago
```

The `overhead.py` utility will annotate traces with information on memory layout, and changes to the working set, eg:
```
malloc(20) = 81d2899a0<8>                         # prev 81d289980, gap 0, next 81d28ba20, gap 8300, pages [8508041, 8508041], 0 more pages 
malloc(96) = 81d21e100<9>                         # prev 81d21e040, gap 104, next 81d220238, gap 8408, pages [8507934, 8507934], 0 more pages 
free(81d2899a0<8>)                                # 20 bytes, allocated 2 lines ago, prev 81d289980, gap 0, next 81d28ba20, gap 8300, pages [8508041-8508041], 0 fewer pages
```

The `prev` is a pointer to a preceding allocated area, `gap` is the distance between end of that allocation and the currently allocated (or freed) pointer, `next` is a pointer to the next allocated area, `gap` is the distance between that and the end of the currently allocated or freed pointer, `pages` is the list of pages in the current allocation; at the end there's information on the difference in the working set.


The `trace2cmt` utility converts the text memory traces into a compact
(~3 bytes per entry on average) memory trace format.

The `cmtreplay` utility takes the compact trace and replays it; use it to "replay" the allocation behaviour with your new C allocator.  You can use the `-v` option to generate a new trace, with the pointers as returned by your custom allocator.
