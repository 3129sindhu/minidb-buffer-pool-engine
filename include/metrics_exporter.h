#pragma once

#include <string>

class BufferPoolManager;

class MetricsExporter {
public:
    static void ExportPrometheus(
        const BufferPoolManager& buffer_pool,
        long insert_count,
        long read_count,
        long scan_count,
        const std::string& output_file
    );
};