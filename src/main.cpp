#include "buffer_pool_manager.h"
#include "disk_manager.h"
#include "free_space_map.h"
#include "page.h"
#include "slotted_page.h"
#include "tuple.h"

#include <iostream>
#include <string>
#include <vector>

EmployeeTuple CreateEmployee(int id) {
    EmployeeTuple emp;
    emp.employee_id = id;
    emp.name = "Employee" + std::to_string(id);
    emp.department = "CS";
    emp.salary = 50000 + id;
    return emp;
}

int main() {
    try {
        DiskManager disk("data/minidb.db");
        BufferPoolManager buffer_pool(3, &disk, 2);

        FreeSpaceMap fsm;

        std::vector<page_id_t> created_pages;

        std::cout << "MiniDB Milestone 5: Buffer Pool Manager with LRU-K"
                  << std::endl;
        std::cout << "--------------------------------------------------"
                  << std::endl;

        for (int id = 1; id <= 500; id++) {
            EmployeeTuple emp = CreateEmployee(id);

            std::vector<char> tuple_bytes =
                TupleSerializer::Serialize(emp);

            int required_space =
                static_cast<int>(tuple_bytes.size()) + 8;

            page_id_t target_page_id =
                fsm.FindPageWithEnoughSpace(required_space);

            Page* page = nullptr;

            if (target_page_id == INVALID_PAGE_ID) {
                page = buffer_pool.NewPage(&target_page_id);

                if (page == nullptr) {
                    std::cout << "[ERROR] Could not allocate new page."
                              << std::endl;
                    return 1;
                }

                created_pages.push_back(target_page_id);

                SlottedPage slotted_page(page->GetData());
                slotted_page.Init();

                fsm.UpdateFreeSpace(
                    target_page_id,
                    slotted_page.GetFreeSpace()
                );

                std::cout << "[FSM] No existing page had enough space. "
                          << "Created page_id=" << target_page_id
                          << std::endl;
            } else {
                page = buffer_pool.FetchPage(target_page_id);

                if (page == nullptr) {
                    std::cout << "[ERROR] Could not fetch page_id="
                              << target_page_id
                              << std::endl;
                    return 1;
                }
            }

            SlottedPage slotted_page(page->GetData());

            int slot_id = -1;

            bool inserted = slotted_page.InsertTuple(
                tuple_bytes.data(),
                static_cast<int>(tuple_bytes.size()),
                slot_id
            );

            if (!inserted) {
                std::cout << "[ERROR] Insert failed. FSM selected page_id="
                          << target_page_id
                          << " but it did not have enough space."
                          << std::endl;
                return 1;
            }

            fsm.UpdateFreeSpace(
                target_page_id,
                slotted_page.GetFreeSpace()
            );

            // We modified the page, so it becomes dirty.
            buffer_pool.UnpinPage(target_page_id, true);

            if (id <= 5 || id % 100 == 0) {
                std::cout << "[INSERT] employee_id=" << emp.employee_id
                          << ", page_id=" << target_page_id
                          << ", slot_id=" << slot_id
                          << ", remaining_free_space="
                          << slotted_page.GetFreeSpace()
                          << " bytes"
                          << std::endl;
            }
        }

        std::cout << std::endl;
        std::cout << "After insert workload:" << std::endl;

        buffer_pool.PrintBufferPool();
        buffer_pool.PrintMetrics();

        std::cout << std::endl;
        fsm.Print();

        if (!created_pages.empty()) {
            page_id_t first_page_id = created_pages.front();

            std::cout << std::endl;
            std::cout << "Read test 1: Fetching first page_id="
                      << first_page_id
                      << std::endl;

            Page* page = buffer_pool.FetchPage(first_page_id);

            if (page != nullptr) {
                SlottedPage slotted_page(page->GetData());
                slotted_page.PrintLayout();

                buffer_pool.UnpinPage(first_page_id, false);
            }

            std::cout << std::endl;
            std::cout << "Read test 2: Fetching same page again to show buffer hit"
                      << std::endl;

            page = buffer_pool.FetchPage(first_page_id);

            if (page != nullptr) {
                buffer_pool.UnpinPage(first_page_id, false);
            }
        }

        std::cout << std::endl;
        std::cout << "Before flushing all pages:" << std::endl;

        buffer_pool.PrintBufferPool();
        buffer_pool.PrintMetrics();

        std::cout << std::endl;
        std::cout << "Flushing all dirty pages..." << std::endl;

        buffer_pool.FlushAllPages();

        std::cout << std::endl;
        std::cout << "After flushing all pages:" << std::endl;

        buffer_pool.PrintBufferPool();
        buffer_pool.PrintMetrics();

        std::cout << std::endl;
        std::cout << "Total pages in database file: "
                  << disk.GetNumPages()
                  << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}