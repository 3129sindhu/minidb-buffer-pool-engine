#include "disk_manager.h"
#include "constants.h"
#include <cstring>
#include <stdexcept>

DiskManager::DiskManager(const std::string& db_file)
    : db_file_(db_file), num_pages_(0) {
    file_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        file_.clear();
        file_.open(db_file_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
    }
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open database file");
    }
    file_.seekg(0, std::ios::end);
    std::streampos file_size = file_.tellg();
    num_pages_ = static_cast<int>(file_size / PAGE_SIZE);
}

DiskManager::~DiskManager() {
    if (file_.is_open()) {
        file_.close();
    }
}

page_id_t DiskManager::AllocatePage() {
    page_id_t new_page_id = num_pages_;
    num_pages_++;
    char empty_page[PAGE_SIZE];
    std::memset(empty_page, 0, PAGE_SIZE);
    WritePage(new_page_id, empty_page);
    return new_page_id;
}

void DiskManager::WritePage(page_id_t page_id, const char* page_data) {
    std::streampos offset = static_cast<std::streampos>(page_id) * PAGE_SIZE;
    file_.seekp(offset);
    file_.write(page_data, PAGE_SIZE);
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write page");
    }
}
void DiskManager::ReadPage(page_id_t page_id, char* page_data) {
    if (page_id < 0 || page_id >= num_pages_) {
        throw std::runtime_error("Invalid page id");
    }
    std::streampos offset = static_cast<std::streampos>(page_id) * PAGE_SIZE;
    file_.seekg(offset);
    file_.read(page_data, PAGE_SIZE);
    if (!file_) {
        throw std::runtime_error("Failed to read page");
    }
}

int DiskManager::GetNumPages() const {
    return num_pages_;
}