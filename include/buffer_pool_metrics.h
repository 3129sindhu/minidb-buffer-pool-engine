#pragma once

struct BufferPoolMetrics {
    long buffer_hits = 0;
    long buffer_misses = 0;

    long disk_reads = 0;
    long disk_writes = 0;

    long evictions = 0;
    long dirty_flushes = 0;

    long pages_allocated = 0;
};