#include "disk_manager.h"
#include "free_space_map.h"
#include "page.h"
#include "slotted_page.h"
#include "tuple.h"

#include <iostream>
#include <string>
#include <unordered_map>
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
        FreeSpaceMap fsm;

        std::vector<Page> pages;
        std::vector<page_id_t> page_ids;
        std::unordered_map<page_id_t, int> page_index;

        auto AllocateNewSlottedPage = [&]() {
            page_id_t new_page_id = disk.AllocatePage();

            pages.emplace_back();
            page_ids.push_back(new_page_id);
            page_index[new_page_id] = static_cast<int>(pages.size()) - 1;

            SlottedPage slotted_page(pages.back().GetData());
            slotted_page.Init();

            fsm.UpdateFreeSpace(new_page_id, slotted_page.GetFreeSpace());

            std::cout << "[ALLOCATE] Created new slotted page. page_id="
                      << new_page_id << std::endl;

            return new_page_id;
        };

        std::cout << "MiniDB Free Space Map Test" << std::endl;
        std::cout << "--------------------------" << std::endl;

        AllocateNewSlottedPage();

        for (int id = 1; id <= 200; id++) {
            EmployeeTuple emp = CreateEmployee(id);
            std::vector<char> tuple_bytes = TupleSerializer::Serialize(emp);

            int required_space = static_cast<int>(tuple_bytes.size()) + 8;

            page_id_t target_page_id = fsm.FindPageWithEnoughSpace(required_space);

            if (target_page_id == INVALID_PAGE_ID) {
                target_page_id = AllocateNewSlottedPage();
            }

            int index = page_index[target_page_id];
            Page& target_page = pages[index];

            SlottedPage slotted_page(target_page.GetData());

            int slot_id;
            bool inserted = slotted_page.InsertTuple(
                tuple_bytes.data(),
                static_cast<int>(tuple_bytes.size()),
                slot_id
            );

            if (!inserted) {
                std::cout << "[ERROR] Page selected by FSM did not have enough space."
                          << std::endl;
                return 1;
            }

            fsm.UpdateFreeSpace(target_page_id, slotted_page.GetFreeSpace());

            disk.WritePage(target_page_id, target_page.GetData());

            if (id <= 5 || id % 50 == 0) {
                std::cout << "[INSERT] employee_id=" << emp.employee_id
                          << " inserted into page_id=" << target_page_id
                          << ", slot_id=" << slot_id
                          << ", tuple_size=" << tuple_bytes.size()
                          << " bytes"
                          << ", remaining_free_space="
                          << slotted_page.GetFreeSpace()
                          << " bytes"
                          << std::endl;
            }
        }

        std::cout << std::endl;
        fsm.Print();

        std::cout << std::endl;
        std::cout << "Layout of first page used in this run:" << std::endl;

        Page read_page;
        page_id_t first_page_id = page_ids[0];

        disk.ReadPage(first_page_id, read_page.GetData());

        SlottedPage read_slotted_page(read_page.GetData());
        read_slotted_page.PrintLayout();

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