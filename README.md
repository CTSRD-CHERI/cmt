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

The `lifetimes.py` utility will annotate the traces with allocation lifetime information, eg:
```
malloc(8) = 81d220b48<6655>
realloc(81d220b48<6655>, 16) = 81d3b83a0<6656>    # 8 bytes, allocated 1 lines ago
free(81d3b83a0<6656>)                             # 16 bytes, allocated 1 lines ago
```

The `trace2cmt` utility converts the text memory traces into a compact
(~3 bytes per entry on average) memory trace format.

The `cmtreplay` utility takes the compact trace and replays it; use it to "replay" the allocation behaviour with your new C allocator.  You can use the `-v` option to generate a new trace, with the pointers as returned by your custom allocator.
