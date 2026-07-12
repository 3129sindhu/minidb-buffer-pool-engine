#include "buffer_pool_manager.h"
#include "demo_event_logger.h"

#include <iomanip>
#include <iostream>

BufferPoolManager::BufferPoolManager(
    int pool_size,
    DiskManager* disk_manager,
    int replacer_k
)
    : pool_size_(pool_size),
      disk_manager_(disk_manager),
      frames_(pool_size),
      replacer_(pool_size, replacer_k),
      current_time_(0) {
    for (int i = 0; i < pool_size_; i++) {
        free_list_.push_back(i);
    }
}

Page* BufferPoolManager::FetchPage(page_id_t page_id) {
    current_time_++;

    auto it = page_table_.find(page_id);

    if (it != page_table_.end()) {
        int frame_id = it->second;
        BufferFrame& frame = frames_[frame_id];

        metrics_.buffer_hits++;

        frame.pin_count++;
        frame.last_used = current_time_;

        replacer_.RecordAccess(frame_id);
        replacer_.SetEvictable(frame_id, false);

        std::cout << "[BUFFER HIT] page_id=" << page_id
                  << " found in frame=" << frame_id
                  << std::endl;

        DemoEventLogger::Log(
            "buffer_hit",
            "Page found in buffer pool memory",
            {
                {"page_id", std::to_string(page_id)},
                {"frame_id", std::to_string(frame_id)},
                {"dirty", frame.is_dirty ? "true" : "false"},
                {"pin_count", std::to_string(frame.pin_count)}
            }
        );

        return &frame.page;
    }

    metrics_.buffer_misses++;

    std::cout << "[BUFFER MISS] page_id=" << page_id
              << " not in memory. Reading from disk."
              << std::endl;

    DemoEventLogger::Log(
        "buffer_miss",
        "Page not found in memory, reading from disk",
        {{"page_id", std::to_string(page_id)}}
    );

    int frame_id = GetAvailableFrame();

    if (frame_id == -1) {
        std::cout << "[BUFFER ERROR] No available frame for page_id="
                  << page_id
                  << std::endl;
        return nullptr;
    }

    BufferFrame& frame = frames_[frame_id];

    disk_manager_->ReadPage(page_id, frame.page.GetData());
    metrics_.disk_reads++;

    frame.page_id = page_id;
    frame.pin_count = 1;
    frame.is_dirty = false;
    frame.last_used = current_time_;

    page_table_[page_id] = frame_id;

    replacer_.RecordAccess(frame_id);
    replacer_.SetEvictable(frame_id, false);

    DemoEventLogger::Log(
        "page_loaded",
        "Page loaded from disk into buffer frame",
        {
            {"page_id", std::to_string(page_id)},
            {"frame_id", std::to_string(frame_id)}
        }
    );

    return &frame.page;
}

Page* BufferPoolManager::NewPage(page_id_t* page_id) {
    current_time_++;

    int frame_id = GetAvailableFrame();

    if (frame_id == -1) {
        std::cout << "[NEW PAGE ERROR] No available frame."
                  << std::endl;
        return nullptr;
    }

    *page_id = disk_manager_->AllocatePage();
    metrics_.pages_allocated++;

    BufferFrame& frame = frames_[frame_id];

    frame.Reset();
    frame.page_id = *page_id;
    frame.pin_count = 1;
    frame.is_dirty = true;
    frame.last_used = current_time_;

    page_table_[*page_id] = frame_id;

    replacer_.RecordAccess(frame_id);
    replacer_.SetEvictable(frame_id, false);

    std::cout << "[NEW PAGE] page_id=" << *page_id
              << " loaded into frame=" << frame_id
              << std::endl;

    DemoEventLogger::Log(
        "page_allocate",
        "New page allocated and loaded into buffer frame",
        {
            {"page_id", std::to_string(*page_id)},
            {"frame_id", std::to_string(frame_id)},
            {"dirty", "true"}
        }
    );

    return &frame.page;
}

bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);

    if (it == page_table_.end()) {
        return false;
    }

    int frame_id = it->second;
    BufferFrame& frame = frames_[frame_id];

    if (frame.pin_count <= 0) {
        return false;
    }

    if (is_dirty) {
        frame.is_dirty = true;
    }

    frame.pin_count--;

    if (frame.pin_count == 0) {
        replacer_.SetEvictable(frame_id, true);
    }

    return true;
}

bool BufferPoolManager::FlushPage(page_id_t page_id) {
    auto it = page_table_.find(page_id);

    if (it == page_table_.end()) {
        return false;
    }

    return FlushFrame(it->second);
}

void BufferPoolManager::FlushAllPages() {
    for (int frame_id = 0; frame_id < pool_size_; frame_id++) {
        FlushFrame(frame_id);
    }
}

void BufferPoolManager::PrintBufferPool() const {
    std::cout << "Buffer Pool State" << std::endl;
    std::cout << "-----------------" << std::endl;
    std::cout << "Frame | Page ID | Pin Count | Dirty | Last Used" << std::endl;

    for (int i = 0; i < pool_size_; i++) {
        const BufferFrame& frame = frames_[i];

        std::cout << std::setw(5) << i << " | "
                  << std::setw(7) << frame.page_id << " | "
                  << std::setw(9) << frame.pin_count << " | "
                  << std::setw(5) << (frame.is_dirty ? "Yes" : "No") << " | "
                  << frame.last_used
                  << std::endl;
    }
}

void BufferPoolManager::PrintMetrics() const {
    long total_accesses = metrics_.buffer_hits + metrics_.buffer_misses;
    double hit_ratio = total_accesses == 0
        ? 0.0
        : static_cast<double>(metrics_.buffer_hits) / total_accesses * 100.0;

    std::cout << "MiniDB Buffer Pool Metrics" << std::endl;
    std::cout << "--------------------------" << std::endl;
    std::cout << "Buffer hits: " << metrics_.buffer_hits << std::endl;
    std::cout << "Buffer misses: " << metrics_.buffer_misses << std::endl;
    std::cout << "Buffer hit ratio: " << std::fixed << std::setprecision(2)
              << hit_ratio << "%" << std::endl;
    std::cout << "Disk reads: " << metrics_.disk_reads << std::endl;
    std::cout << "Disk writes: " << metrics_.disk_writes << std::endl;
    std::cout << "Evictions: " << metrics_.evictions << std::endl;
    std::cout << "Dirty flushes: " << metrics_.dirty_flushes << std::endl;
    std::cout << "Pages allocated: " << metrics_.pages_allocated << std::endl;
    std::cout << "Current dirty pages: " << GetDirtyPageCount() << std::endl;
    std::cout << "Current pinned pages: " << GetPinnedPageCount() << std::endl;
    std::cout << "Used frames: " << GetUsedFrameCount()
              << "/" << pool_size_
              << std::endl;
}

void BufferPoolManager::PrintLRUK() const {
    replacer_.Print();
}

const BufferPoolMetrics& BufferPoolManager::GetMetrics() const {
    return metrics_;
}

int BufferPoolManager::GetDirtyPageCount() const {
    int count = 0;

    for (const BufferFrame& frame : frames_) {
        if (frame.page_id != INVALID_PAGE_ID && frame.is_dirty) {
            count++;
        }
    }

    return count;
}

int BufferPoolManager::GetPinnedPageCount() const {
    int count = 0;

    for (const BufferFrame& frame : frames_) {
        if (frame.page_id != INVALID_PAGE_ID && frame.pin_count > 0) {
            count++;
        }
    }

    return count;
}

int BufferPoolManager::GetUsedFrameCount() const {
    int count = 0;

    for (const BufferFrame& frame : frames_) {
        if (frame.page_id != INVALID_PAGE_ID) {
            count++;
        }
    }

    return count;
}

int BufferPoolManager::GetPoolSize() const {
    return pool_size_;
}

bool BufferPoolManager::ContainsPage(page_id_t page_id) const {
    return page_table_.find(page_id) != page_table_.end();
}

int BufferPoolManager::GetAvailableFrame() {
    if (!free_list_.empty()) {
        int frame_id = free_list_.back();
        free_list_.pop_back();
        frames_[frame_id].Reset();
        return frame_id;
    }

    int victim_frame = -1;

    if (!replacer_.Evict(&victim_frame)) {
        return -1;
    }

    page_id_t victim_page = frames_[victim_frame].page_id;
    bool victim_dirty = frames_[victim_frame].is_dirty;

    std::cout << "[EVICT] frame=" << victim_frame
              << ", page_id=" << victim_page
              << ", dirty=" << (victim_dirty ? "Yes" : "No")
              << std::endl;

    DemoEventLogger::Log(
        "lruk_eviction",
        "LRU-K selected a victim frame",
        {
            {"victim_frame", std::to_string(victim_frame)},
            {"victim_page", std::to_string(victim_page)},
            {"dirty", victim_dirty ? "true" : "false"}
        }
    );

    if (victim_dirty) {
        FlushFrame(victim_frame);
    }

    if (victim_page != INVALID_PAGE_ID) {
        page_table_.erase(victim_page);
    }
    metrics_.evictions++;
    frames_[victim_frame].Reset();
    return victim_frame;
}
bool BufferPoolManager::FlushFrame(int frame_id) {
    if (frame_id < 0 || frame_id >= pool_size_) {
        return false;
    }
    BufferFrame& frame = frames_[frame_id];
    if (frame.page_id == INVALID_PAGE_ID || !frame.is_dirty) {
        return true;
    }
    DemoEventLogger::Log(
        "dirty_flush",
        "Dirty page flushed to disk",
        {
            {"page_id", std::to_string(frame.page_id)},
            {"frame_id", std::to_string(frame_id)}
        }
    );
    disk_manager_->WritePage(frame.page_id, frame.page.GetData());
    metrics_.disk_writes++;
    metrics_.dirty_flushes++;

    frame.is_dirty = false;
    std::cout << "[FLUSH FRAME] page_id=" << frame.page_id
              << " from frame=" << frame_id
              << std::endl;
    return true;
}
