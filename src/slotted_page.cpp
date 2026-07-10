#include "slotted_page.h"

#include <cstring>
#include <iostream>

SlottedPage::SlottedPage(char* page_data)
    : data_(page_data) {}

void SlottedPage::Init() {
    SetSlotCount(0);
    SetFreeSpacePointer(PAGE_SIZE);
}
bool SlottedPage::InsertTuple(const char* tuple_data, int tuple_size, int& slot_id) {
    int required_space = tuple_size + SLOT_SIZE;
    if (GetFreeSpace() < required_space) {
        return false;
    }
int current_free_pointer = GetFreeSpacePointer();
    int tuple_offset = current_free_pointer - tuple_size;
    std::memcpy(data_ + tuple_offset, tuple_data, tuple_size);

    slot_id = GetSlotCount();
SetSlot(slot_id, tuple_offset, tuple_size);

    
    SetSlotCount(slot_id + 1);
    SetFreeSpacePointer(tuple_offset);

    return true;
}
bool SlottedPage::GetTuple(int slot_id, std::vector<char>& tuple_data) const {
    if (slot_id < 0 || slot_id >= GetSlotCount()) {
        return false;
    }
    if (IsDeleted(slot_id)) {
        return false;
    }
    int tuple_offset = GetSlotOffset(slot_id);
    int tuple_size = GetSlotSize(slot_id);
    tuple_data.resize(tuple_size);
    std::memcpy(tuple_data.data(), data_ + tuple_offset, tuple_size);
    return true;
}
bool SlottedPage::DeleteTuple(int slot_id) {
    if (slot_id < 0 || slot_id >= GetSlotCount()) {
        return false;
    }
    int tuple_size = GetSlotSize(slot_id);
    if (tuple_size <= 0) {
        return false;
    }
    SetSlotSize(slot_id, -tuple_size);
    return true;
}

bool SlottedPage::IsDeleted(int slot_id) const {
    if (slot_id < 0 || slot_id >= GetSlotCount()) {
        return true;
    }
    return GetSlotSize(slot_id) <= 0;
}

int SlottedPage::GetSlotCount() const {
    return ReadInt(SLOT_COUNT_OFFSET);
}

int SlottedPage::GetFreeSpace() const {
    int slot_array_end = HEADER_SIZE + GetSlotCount() * SLOT_SIZE;
int free_space_pointer = GetFreeSpacePointer();

    return free_space_pointer - slot_array_end;
}

void SlottedPage::PrintLayout() const {
    std::cout << "Slotted Page Layout" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << "Page size: " << PAGE_SIZE << " bytes" << std::endl;
    std::cout << "Slot count: " << GetSlotCount() << std::endl;
    std::cout << "Free space pointer: " << GetFreeSpacePointer() << std::endl;
    std::cout << "Free space: " << GetFreeSpace() << " bytes" << std::endl;
    std::cout << std::endl;
    std::cout << "Slots:" << std::endl;
    for (int i = 0; i < GetSlotCount(); i++) {
        int tuple_offset = GetSlotOffset(i);
        int tuple_size = GetSlotSize(i);
        if (tuple_size < 0) {
            std::cout << "  Slot " << i
                      << " -> DELETED"
                      << ", offset: " << tuple_offset
                      << ", old size: " << -tuple_size
                      << " bytes"
                      << std::endl;
        } else {
            std::cout << "  Slot " << i
                      << " -> ACTIVE"
                      << ", offset: " << tuple_offset
                      << ", size: " << tuple_size
                      << " bytes"
                      << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "Note: deleted slots are tombstones. Space is not physically compacted yet."
              << std::endl;
}

int SlottedPage::ReadInt(int offset) const {
    int value;
    std::memcpy(&value, data_ + offset, sizeof(int));
    return value;
}

void SlottedPage::WriteInt(int offset, int value) {
    std::memcpy(data_ + offset, &value, sizeof(int));
}

int SlottedPage::GetFreeSpacePointer() const {
    return ReadInt(FREE_SPACE_POINTER_OFFSET);
}

void SlottedPage::SetFreeSpacePointer(int value) {
    WriteInt(FREE_SPACE_POINTER_OFFSET, value);
}

void SlottedPage::SetSlotCount(int value) {
    WriteInt(SLOT_COUNT_OFFSET, value);
}

int SlottedPage::GetSlotOffset(int slot_id) const {
    int slot_position = HEADER_SIZE + slot_id * SLOT_SIZE;
    return ReadInt(slot_position);
}

int SlottedPage::GetSlotSize(int slot_id) const {
    int slot_position = HEADER_SIZE + slot_id * SLOT_SIZE;
    return ReadInt(slot_position + 4);
}

void SlottedPage::SetSlot(int slot_id, int tuple_offset, int tuple_size) {
    int slot_position = HEADER_SIZE + slot_id * SLOT_SIZE;

    WriteInt(slot_position, tuple_offset);
    WriteInt(slot_position + 4, tuple_size);
}
void SlottedPage::SetSlotSize(int slot_id, int tuple_size) {
    int slot_position = HEADER_SIZE + slot_id * SLOT_SIZE;
    WriteInt(slot_position + 4, tuple_size);
}