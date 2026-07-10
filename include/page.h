#pragma once
#include "constants.h"
#include <array>
#include <cstring>

class Page {
public:
    Page() {
        ResetMemory();
    }
    char* GetData() {
        return data_.data();
    }
    const char* GetData() const {
        return data_.data();
    }
    void ResetMemory() {
        std::memset(data_.data(), 0, PAGE_SIZE);
    }
private:
    std::array<char, PAGE_SIZE> data_;
};