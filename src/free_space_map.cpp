#include "free_space_map.h"

#include <iostream>

void FreeSpaceMap::UpdateFreeSpace(page_id_t page_id, int free_space) {
    free_space_[page_id] = free_space;
}

page_id_t FreeSpaceMap::FindPageWithEnoughSpace(int required_space) const {
    for (const auto& entry : free_space_) {
        page_id_t page_id = entry.first;
        int available_space = entry.second;

        if (available_space >= required_space) {
            return page_id;
        }
    }

    return INVALID_PAGE_ID;
}

int FreeSpaceMap::GetFreeSpace(page_id_t page_id) const {
    auto it = free_space_.find(page_id);

    if (it == free_space_.end()) {
        return 0;
    }

    return it->second;
}

void FreeSpaceMap::Print() const {
    std::cout << "Free Space Map" << std::endl;
    std::cout << "--------------" << std::endl;
    std::cout << "Page ID | Free Space" << std::endl;

    for (const auto& entry : free_space_) {
        std::cout << entry.first << "       | "
                  << entry.second << " bytes" << std::endl;
    }
}