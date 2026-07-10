#include "buffer_pool_manager.h"

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
    auto it = page_table_.find(page_id);

    if (it != page_table_.end()) {
int frame_id = it->second;
BufferFrame& frame = frames_[frame_id];

        frame.pin_count++;
frame.last_used = ++current_time_;

replacer_.RecordAccess(frame_id);
replacer_.SetEvictable(frame_id, false);
        metrics_.buffer_hits++;
        std::cout << "[BUFFER HIT] page_id=" << page_id<< " found in frame=" << frame_id<< std::endl;
        return &frame.page;
    }
    metrics_.buffer_misses++;

    std::cout << "[BUFFER MISS] page_id=" << page_id<< " not in memory" << std::endl;

    int frame_id = GetAvailableFrame();
    if (frame_id == -1) {
        std::cout << "[FETCH FAILED] No available frame. All pages are pinned."
                  << std::endl;
        return nullptr;
    }

    BufferFrame& frame = frames_[frame_id];

    frame.Reset();

    disk_manager_->ReadPage(page_id, frame.page.GetData());
    metrics_.disk_reads++;
    frame.page_id = page_id;
    frame.pin_count = 1;

    frame.is_dirty = false;
    frame.last_used = ++current_time_;
    page_table_[page_id] = frame_id;

    replacer_.RecordAccess(frame_id);

    replacer_.SetEvictable(frame_id, false);
    std::cout << "[DISK READ] Loaded page_id=" << page_id<< " into frame=" << frame_id<< std::endl;
    return &frame.page;
}
Page* BufferPoolManager::NewPage(page_id_t* page_id) {
    int frame_id = GetAvailableFrame();
    if (frame_id == -1) {
        std::cout << "[NEW PAGE FAILED] No available frame. All pages are pinned."<< std::endl;
        return nullptr;
    }
    page_id_t new_page_id = disk_manager_->AllocatePage();

    metrics_.pages_allocated++;
    metrics_.disk_writes++;
    BufferFrame& frame = frames_[frame_id];

    frame.Reset();
    frame.page_id = new_page_id;
    frame.pin_count = 1;
    frame.is_dirty = false;
    
    frame.last_used = ++current_time_;

    page_table_[new_page_id] = frame_id;
    replacer_.RecordAccess(frame_id);
    replacer_.SetEvictable(frame_id, false);

    *page_id = new_page_id;

    std::cout << "[NEW PAGE] page_id=" << new_page_id
              << " placed in frame=" << frame_id
              << std::endl;

    return &frame.page;
}
bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);

    if (it == page_table_.end()) {
        std::cout << "[UNPIN ERROR] page_id=" << page_id
                  << " not found in buffer pool"
                  << std::endl;
        return false;
    }

    int frame_id = it->second;
    BufferFrame& frame = frames_[frame_id];

    if (frame.pin_count <= 0) {
        std::cout << "[UNPIN ERROR] page_id=" << page_id
                  << " already has pin_count=0"
                  << std::endl;
        return false;
    }

    frame.pin_count--;

    if (is_dirty) {
        frame.is_dirty = true;
    }

    frame.last_used = ++current_time_;

    if (frame.pin_count == 0) {
        replacer_.SetEvictable(frame_id, true);
    }

    std::cout << "[UNPIN] page_id=" << page_id
              << ", frame=" << frame_id
              << ", pin_count=" << frame.pin_count
              << ", dirty=" << (frame.is_dirty ? "true" : "false")
              << std::endl;

    return true;
}
bool BufferPoolManager::FlushPage(page_id_t page_id) {
    auto it = page_table_.find(page_id);

    if (it == page_table_.end()) {
        std::cout << "[FLUSH ERROR] page_id=" << page_id
                  << " not found in buffer pool"
                  << std::endl;
        return false;
    }

    int frame_id = it->second;
    return FlushFrame(frame_id);
}
void BufferPoolManager::FlushAllPages() {
    for (int frame_id = 0; frame_id < pool_size_; frame_id++) {
        if (frames_[frame_id].page_id != INVALID_PAGE_ID) {
            FlushFrame(frame_id);
        }
    }
}
void BufferPoolManager::PrintBufferPool() const {
    std::cout << std::endl;
    std::cout << "Buffer Pool State" << std::endl;
    std::cout << "-----------------" << std::endl;
    std::cout << "Frame | Page ID | Pin Count | Dirty | Last Used" << std::endl;

    for (int i = 0; i < pool_size_; i++) {
        const BufferFrame& frame = frames_[i];

        std::cout << std::setw(5) << i << " | "<< std::setw(7) << frame.page_id << " | "<< std::setw(9) << frame.pin_count << " | " << std::setw(5) << (frame.is_dirty ? "Yes" : "No") << " | "<< frame.last_used<< std::endl;
    }
}
void BufferPoolManager::PrintMetrics() const {
    long total_accesses = metrics_.buffer_hits + metrics_.buffer_misses;

    double hit_ratio = 0.0;

    if (total_accesses > 0) {
        hit_ratio =
            static_cast<double>(metrics_.buffer_hits) /
            static_cast<double>(total_accesses) *
            100.0;
    }

    std::cout << std::endl;
    std::cout << "MiniDB Buffer Pool Metrics" << std::endl;
    std::cout << "--------------------------" << std::endl;
    std::cout << "Buffer hits: " << metrics_.buffer_hits << std::endl;
    std::cout << "Buffer misses: " << metrics_.buffer_misses << std::endl;
    std::cout << "Buffer hit ratio: "
              << std::fixed << std::setprecision(2)
              << hit_ratio << "%" << std::endl;
    std::cout << "Disk reads: " << metrics_.disk_reads << std::endl;
    std::cout << "Disk writes: " << metrics_.disk_writes << std::endl;
    std::cout << "Evictions: " << metrics_.evictions << std::endl;
    std::cout << "Dirty flushes: " << metrics_.dirty_flushes << std::endl;
    std::cout << "Pages allocated: " << metrics_.pages_allocated << std::endl;
    std::cout << "Current dirty pages: " << GetDirtyPageCount() << std::endl;
    std::cout << "Current pinned pages: " << GetPinnedPageCount() << std::endl;
    std::cout << "Used frames: " << GetUsedFrameCount()
              << "/" << pool_size_ << std::endl;
}
const BufferPoolMetrics& BufferPoolManager::GetMetrics() const {
    return metrics_;
}
int BufferPoolManager::GetDirtyPageCount() const {
    int count = 0;

    for (const auto& frame : frames_) {
        if (frame.page_id != INVALID_PAGE_ID && frame.is_dirty) {
            count++;
        }
    }

    return count;
}
int BufferPoolManager::GetPinnedPageCount() const {
    int count = 0;

    for (const auto& frame : frames_) {
        if (frame.page_id != INVALID_PAGE_ID && frame.pin_count > 0) {
            count++;
        }
    }

    return count;
}
int BufferPoolManager::GetUsedFrameCount() const {
    int count = 0;

    for (const auto& frame : frames_) {
        if (frame.page_id != INVALID_PAGE_ID) {
            count++;
        }
    }

    return count;
}
int BufferPoolManager::GetAvailableFrame() {
    if (!free_list_.empty()) {
        int frame_id = free_list_.back();
        free_list_.pop_back();

        std::cout << "[FREE FRAME] Using free frame=" << frame_id
                  << std::endl;

        return frame_id;
    }

    int victim_frame_id = -1;

    bool found_victim = replacer_.Evict(&victim_frame_id);

    if (!found_victim) {
        return -1;
    }

    BufferFrame& victim = frames_[victim_frame_id];

    std::cout << "[EVICT] Evicting page_id=" << victim.page_id
              << " from frame=" << victim_frame_id
              << std::endl;

    metrics_.evictions++;

    if (victim.is_dirty) {
        std::cout << "[DIRTY PAGE] page_id=" << victim.page_id
                  << " must be flushed before eviction"
                  << std::endl;

        FlushFrame(victim_frame_id);
    }

    page_table_.erase(victim.page_id);

    victim.Reset();

    return victim_frame_id;
}
bool BufferPoolManager::FlushFrame(int frame_id) {
    if (frame_id < 0 || frame_id >= pool_size_) {
        return false;
    }

    BufferFrame& frame = frames_[frame_id];

    if (frame.page_id == INVALID_PAGE_ID) {
        return false;
    }

    if (!frame.is_dirty) {
        return true;
    }

    disk_manager_->WritePage(frame.page_id, frame.page.GetData());

    metrics_.disk_writes++;
    metrics_.dirty_flushes++;

    std::cout << "[DISK WRITE] Flushed page_id=" << frame.page_id<< " from frame=" << frame_id << " to disk"
              << std::endl;

    frame.is_dirty = false;

    return true;
}

int BufferPoolManager::GetPoolSize() const {
    return pool_size_;
}
bool BufferPoolManager::ContainsPage(page_id_t page_id) const {
    return page_table_.find(page_id) != page_table_.end();
}
void BufferPoolManager::PrintLRUK() const {
    replacer_.Print();
}