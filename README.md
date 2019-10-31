# cmt

The log2trace.sh converts the dtrace memory logs into memory traces.

The kdump2trace.sh generates memory traces from kdump(1) output.

The trace2cmt utility converts the text memory traces into a compact (~3 bytes per entry on average) memory trace format.

The cmtreplay utility takes the compact trace and replays it.

# dtrace malloc traces

Malloc tracing tools: https://github.com/CTSRD-CHERI/memory-alloc-tracing

Malloc traces: https://github.com/CTSRD-CHERI/memory-alloc-tracing/wiki/Trace-directory

