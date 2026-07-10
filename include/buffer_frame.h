#pragma once

#include "constants.h"
#include "page.h"

struct BufferFrame {
    Page page;

    page_id_t page_id = INVALID_PAGE_ID;

    int pin_count = 0;

    bool is_dirty = false;

    long last_used = 0;

    void Reset() {
        page.ResetMemory();
        page_id = INVALID_PAGE_ID;
        pin_count = 0;
        is_dirty = false;
        last_used = 0;
    }
};