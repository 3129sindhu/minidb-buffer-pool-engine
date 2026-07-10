#include "disk_manager.h"
#include "page.h"
#include "tuple.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int main() {
    try {
        DiskManager disk("data/minidb.db");

        EmployeeTuple emp;
        emp.employee_id = 101;
        emp.name = "Sindhu";
        emp.department = "CS";
        emp.salary = 70000;

        std::vector<char> serialized_tuple = TupleSerializer::Serialize(emp);

        Page page;

        page_id_t page_id = disk.AllocatePage();

        std::memcpy(page.GetData(), serialized_tuple.data(), serialized_tuple.size());

        disk.WritePage(page_id, page.GetData());

        Page read_page;
        disk.ReadPage(page_id, read_page.GetData());

        EmployeeTuple decoded =
            TupleSerializer::Deserialize(read_page.GetData(), serialized_tuple.size());

        std::cout << "MiniDB Tuple Serialization Test" << std::endl;
        std::cout << "-------------------------------" << std::endl;
        std::cout << "Allocated page id: " << page_id << std::endl;
        std::cout << "Serialized tuple size: " << serialized_tuple.size() << " bytes" << std::endl;

        std::cout << "Data read from disk:" << std::endl;
        std::cout << "Employee ID: " << decoded.employee_id << std::endl;
        std::cout << "Name: " << decoded.name << std::endl;
        std::cout << "Department: " << decoded.department << std::endl;
        std::cout << "Salary: " << decoded.salary << std::endl;

        std::cout << "Total pages in database file: " << disk.GetNumPages() << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}