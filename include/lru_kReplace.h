#pragma once

#include <deque>
#include <vector>

class LRUKReplacer {
public:
    LRUKReplacer(int num_frames, int k);

    void RecordAccess(int frame_id);

    void SetEvictable(int frame_id, bool evictable);

    bool Evict(int* frame_id);

    void Remove(int frame_id);

    int Size() const;

    void Print() const;

private:
    struct FrameHistory {
        std::deque<long> access_history;
        bool evictable = false;
        bool exists = false;
    };

    int num_frames_;
    int k_;
    long current_timestamp_;
    int curr_size_;

    std::vector<FrameHistory> histories_;
};
