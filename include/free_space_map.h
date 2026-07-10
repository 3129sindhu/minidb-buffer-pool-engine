#pragma once
#include "constants.h"
#include <unordered_map>
class FreeSpaceMap {
public:
    void UpdateFreeSpace(page_id_t page_id, int free_space);
    page_id_t FindPageWithEnoughSpace(int required_space) const;
    int GetFreeSpace(page_id_t page_id) const;
    void Print() const;
private:
    std::unordered_map<page_id_t, int> free_space_;
};