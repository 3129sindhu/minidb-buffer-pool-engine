#pragma once

#include "constants.h"
#include <fstream>
#include <string>
class DiskManager {
public:
    explicit DiskManager(const std::string& db_file);
    ~DiskManager();
    page_id_t AllocatePage();
    void WritePage(page_id_t page_id, const char* page_data);
    void ReadPage(page_id_t page_id, char* page_data);
    int GetNumPages() const;

private:
    std::string db_file_;
    std::fstream file_;
    int num_pages_;
};