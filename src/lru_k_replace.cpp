#include "lru_kReplace.h"

#include <iostream>
#include <limits>

LRUKReplacer::LRUKReplacer(int num_frames, int k)
    : num_frames_(num_frames),
      k_(k),
      current_timestamp_(0),
      curr_size_(0),
      histories_(num_frames) {}

void LRUKReplacer::RecordAccess(int frame_id) {
    if (frame_id < 0 || frame_id >= num_frames_) {
        return;
    }

    current_timestamp_++;

    FrameHistory& history = histories_[frame_id];

    history.exists = true;
    history.access_history.push_back(current_timestamp_);

    if (static_cast<int>(history.access_history.size()) > k_) {
        history.access_history.pop_front();
    }

    std::cout << "[LRU-K] RecordAccess frame=" << frame_id
              << ", timestamp=" << current_timestamp_
              << std::endl;
}

void LRUKReplacer::SetEvictable(int frame_id, bool evictable) {
    if (frame_id < 0 || frame_id >= num_frames_) {
        return;
    }

    FrameHistory& history = histories_[frame_id];

    if (!history.exists) {
        return;
    }

    if (history.evictable == evictable) {
        return;
    }

    if (evictable) {
        curr_size_++;
    } else {
        curr_size_--;
    }

    history.evictable = evictable;

    std::cout << "[LRU-K] SetEvictable frame=" << frame_id
              << ", evictable=" << (evictable ? "true" : "false")
              << std::endl;
}

bool LRUKReplacer::Evict(int* frame_id) {
    int victim = -1;

    bool victim_has_infinite_distance = false;
    long victim_kth_timestamp = std::numeric_limits<long>::max();
    long victim_earliest_timestamp = std::numeric_limits<long>::max();

    for (int i = 0; i < num_frames_; i++) {
        const FrameHistory& history = histories_[i];

        if (!history.exists || !history.evictable || history.access_history.empty()) {
            continue;
        }

        bool has_infinite_distance =
            static_cast<int>(history.access_history.size()) < k_;

        long earliest_timestamp = history.access_history.front();

        long kth_timestamp;
        if (has_infinite_distance) {
            kth_timestamp = std::numeric_limits<long>::min();
        } else {
            kth_timestamp = history.access_history.front();
        }

        bool choose_this = false;

        if (victim == -1) {
            choose_this = true;
        } else if (has_infinite_distance && !victim_has_infinite_distance) {
            choose_this = true;
        } else if (has_infinite_distance && victim_has_infinite_distance) {
            if (earliest_timestamp < victim_earliest_timestamp) {
                choose_this = true;
            }
        } else if (!has_infinite_distance && !victim_has_infinite_distance) {
            if (kth_timestamp < victim_kth_timestamp) {
                choose_this = true;
            } else if (kth_timestamp == victim_kth_timestamp &&
                       earliest_timestamp < victim_earliest_timestamp) {
                choose_this = true;
            }
        }

        if (choose_this) {
            victim = i;
            victim_has_infinite_distance = has_infinite_distance;
            victim_kth_timestamp = kth_timestamp;
            victim_earliest_timestamp = earliest_timestamp;
        }
    }

    if (victim == -1) {
        return false;
    }

    *frame_id = victim;

    std::cout << "[LRU-K] Evict selected frame=" << victim << std::endl;

    Remove(victim);

    return true;
}

void LRUKReplacer::Remove(int frame_id) {
    if (frame_id < 0 || frame_id >= num_frames_) {
        return;
    }

    FrameHistory& history = histories_[frame_id];

    if (!history.exists) {
        return;
    }

    if (history.evictable) {
        curr_size_--;
    }

    history.access_history.clear();
    history.evictable = false;
    history.exists = false;
}

int LRUKReplacer::Size() const {
    return curr_size_;
}