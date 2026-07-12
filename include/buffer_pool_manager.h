#pragma once

#include "buffer_frame.h"
#include "buffer_pool_metrics.h"
#include "constants.h"
#include "disk_manager.h"
#include "lru_kReplace.h"
#include "page.h"

#include <unordered_map>
#include <vector>

class BufferPoolManager {
public:
    BufferPoolManager(int pool_size, DiskManager* disk_manager, int replacer_k = 2);

    Page* FetchPage(page_id_t page_id);

    Page* NewPage(page_id_t* page_id);

    bool UnpinPage(page_id_t page_id, bool is_dirty);

    bool FlushPage(page_id_t page_id);

    void FlushAllPages();

    void PrintBufferPool() const;

    void PrintMetrics() const;

    void PrintLRUK() const;

    const BufferPoolMetrics& GetMetrics() const;

    int GetDirtyPageCount() const;

    int GetPinnedPageCount() const;

    int GetUsedFrameCount() const;

    int GetPoolSize() const;

    bool ContainsPage(page_id_t page_id) const;

private:
    int GetAvailableFrame();

    bool FlushFrame(int frame_id);

    int pool_size_;

    DiskManager* disk_manager_;

    std::vector<BufferFrame> frames_;

    std::vector<int> free_list_;

    std::unordered_map<page_id_t, int> page_table_;

    LRUKReplacer replacer_;

    long current_time_;

    BufferPoolMetrics metrics_;
};
