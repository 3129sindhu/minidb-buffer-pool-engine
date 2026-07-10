#include "metrics_exporter.h"
#include "buffer_pool_manager.h"
#include "buffer_pool_metrics.h"

#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void WriteCounter(std::ofstream& out,
                  const std::string& name,
                  const std::string& help,
                  long value) {
    out << "# HELP " << name << " " << help << "\n";
    out << "# TYPE " << name << " counter\n";
    out << name << " " << value << "\n\n";
}

void WriteGauge(std::ofstream& out,
                const std::string& name,
                const std::string& help,
                double value) {
    out << "# HELP " << name << " " << help << "\n";
    out << "# TYPE " << name << " gauge\n";
    out << name << " " << value << "\n\n";
}

}

void MetricsExporter::ExportPrometheus(
    const BufferPoolManager& buffer_pool,
    long insert_count,
    long read_count,
    long scan_count,
    const std::string& output_file
) {
    std::ofstream out(output_file);

    if (!out.is_open()) {
        throw std::runtime_error("Could not open metrics output file: " + output_file);
    }

    const BufferPoolMetrics& metrics = buffer_pool.GetMetrics();

    long total_buffer_accesses =
        metrics.buffer_hits + metrics.buffer_misses;

    double hit_ratio = 0.0;

    if (total_buffer_accesses > 0) {
        hit_ratio =
            static_cast<double>(metrics.buffer_hits) /
            static_cast<double>(total_buffer_accesses);
    }

    WriteCounter(
        out,
        "minidb_buffer_hits_total",
        "Total number of buffer pool hits.",
        metrics.buffer_hits
    );

    WriteCounter(
        out,
        "minidb_buffer_misses_total",
        "Total number of buffer pool misses.",
        metrics.buffer_misses
    );

    WriteGauge(
        out,
        "minidb_buffer_hit_ratio",
        "Current buffer pool hit ratio from 0 to 1.",
        hit_ratio
    );

    WriteCounter(
        out,
        "minidb_disk_reads_total",
        "Total number of disk page reads.",
        metrics.disk_reads
    );

    WriteCounter(
        out,
        "minidb_disk_writes_total",
        "Total number of disk page writes.",
        metrics.disk_writes
    );

    WriteCounter(
        out,
        "minidb_evictions_total",
        "Total number of buffer pool evictions.",
        metrics.evictions
    );

    WriteCounter(
        out,
        "minidb_dirty_flushes_total",
        "Total number of dirty page flushes.",
        metrics.dirty_flushes
    );

    WriteCounter(
        out,
        "minidb_pages_allocated_total",
        "Total number of pages allocated by MiniDB.",
        metrics.pages_allocated
    );

    WriteCounter(
        out,
        "minidb_inserts_total",
        "Total number of insert operations.",
        insert_count
    );

    WriteCounter(
        out,
        "minidb_reads_total",
        "Total number of read operations.",
        read_count
    );

    WriteCounter(
        out,
        "minidb_scans_total",
        "Total number of scan operations.",
        scan_count
    );

    WriteGauge(
        out,
        "minidb_dirty_pages",
        "Current number of dirty pages in the buffer pool.",
        buffer_pool.GetDirtyPageCount()
    );

    WriteGauge(
        out,
        "minidb_pinned_pages",
        "Current number of pinned pages in the buffer pool.",
        buffer_pool.GetPinnedPageCount()
    );

    WriteGauge(
        out,
        "minidb_used_frames",
        "Current number of used frames in the buffer pool.",
        buffer_pool.GetUsedFrameCount()
    );

    WriteGauge(
        out,
        "minidb_buffer_pool_size",
        "Total number of frames in the buffer pool.",
        buffer_pool.GetPoolSize()
    );

    WriteGauge(
        out,
        "minidb_free_frames",
        "Current number of free frames in the buffer pool.",
        buffer_pool.GetPoolSize() - buffer_pool.GetUsedFrameCount()
    );
}