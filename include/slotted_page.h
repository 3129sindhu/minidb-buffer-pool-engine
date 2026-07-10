#pragma once
#include "constants.h"
#include <vector>
class SlottedPage {
public:
    explicit SlottedPage(char* page_data);
    void Init();
    bool InsertTuple(const char* tuple_data, int tuple_size, int& slot_id);
    bool GetTuple(int slot_id, std::vector<char>& tuple_data) const;
    int GetSlotCount() const;
    int GetFreeSpace() const;
    void PrintLayout() const;
    bool DeleteTuple(int slot_id);
bool IsDeleted(int slot_id) const;

private:
    static constexpr int HEADER_SIZE = 8;
    static constexpr int SLOT_SIZE = 8;
    static constexpr int SLOT_COUNT_OFFSET = 0;
    static constexpr int FREE_SPACE_POINTER_OFFSET = 4;

    char* data_;
    int ReadInt(int offset) const;
    void WriteInt(int offset, int value);
    int GetFreeSpacePointer() const;

    void SetFreeSpacePointer(int value);
    void SetSlotCount(int value);
    int GetSlotOffset(int slot_id) const;
    int GetSlotSize(int slot_id) const;
    void SetSlot(int slot_id, int tuple_offset, int tuple_size);
    void SetSlotSize(int slot_id, int tuple_size);
};