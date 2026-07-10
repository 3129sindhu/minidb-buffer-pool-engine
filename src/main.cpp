#include "disk_manager.h"
#include "page.h"

#include <cstring>
#include <iostream>
#include <string>

int main() {
    try {
        DiskManager disk("data/minidb.db");

        Page page;

        page_id_t page_id = disk.AllocatePage();

        std::string message = "Hello from MiniDB disk page";

        std::memcpy(page.GetData(), message.c_str(), message.size());

        disk.WritePage(page_id, page.GetData());

        Page read_page;
        disk.ReadPage(page_id, read_page.GetData());

        std::cout << "MiniDB Disk Manager Test" << std::endl;
        std::cout << "------------------------" << std::endl;
        std::cout << "Allocated page id: " << page_id << std::endl;
        std::cout << "Data read from disk: " << read_page.GetData() << std::endl;
        std::cout << "Total pages in database file: " << disk.GetNumPages() << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}