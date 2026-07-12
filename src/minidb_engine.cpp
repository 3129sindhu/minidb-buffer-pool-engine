#include "minidb_engine.h"

#include "demo_event_logger.h"
#include "slotted_page.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

MiniDBEngine::MiniDBEngine(
    const std::string& db_file,
    int buffer_pool_size,
    int replacer_k
)
    : disk_(db_file),
      buffer_pool_(buffer_pool_size, &disk_, replacer_k),
      insert_count_(0),
      read_count_(0),
      scan_count_(0),
      delete_count_(0),
      next_generated_id_(1000),
      metrics_file_("data/metrics.prom") {
    ExportMetrics();
}

bool MiniDBEngine::InsertEmployee(
    int employee_id,
    const std::string& name,
    const std::string& department,
    int salary
) {
    EmployeeTuple emp;
    emp.employee_id = employee_id;
    emp.name = name;
    emp.department = department;
    emp.salary = salary;

    return InsertEmployeeTuple(emp, true);
}

bool MiniDBEngine::InsertLargeEmployee(int employee_id) {
    EmployeeTuple emp;
    emp.employee_id = employee_id;
    emp.name = "EMP_" + std::to_string(employee_id) + "_" + std::string(3980, 'X');
    emp.department = "LRUK";
    emp.salary = 50000 + employee_id;

    bool inserted = InsertEmployeeTuple(emp, false);

    if (inserted) {
        std::cout << "[INSERTBIG] employee_id=" << employee_id
                  << " inserted as a large demo tuple"
                  << std::endl;
        std::cout << "[INSERTBIG] This tuple is large enough to force page-level behavior."
                  << std::endl;
    }

    ExportMetrics();
    return inserted;
}

bool MiniDBEngine::InsertEmployeeTuple(
    const EmployeeTuple& emp,
    bool print_result
) {
    if (employee_index_.find(emp.employee_id) != employee_index_.end()) {
        std::cout << "[INSERT ERROR] employee_id="
                  << emp.employee_id
                  << " already exists in this MiniDB run."
                  << std::endl;
        return false;
    }

    std::vector<char> tuple_bytes = TupleSerializer::Serialize(emp);
    int required_space = static_cast<int>(tuple_bytes.size()) + 8;

    DemoEventLogger::Log(
        "serialize",
        "Record serialized into bytes for insert",
        {
            {"employee_id", std::to_string(emp.employee_id)},
            {"name", emp.name.size() > 30 ? emp.name.substr(0, 30) + "..." : emp.name},
            {"department", emp.department},
            {"salary", std::to_string(emp.salary)},
            {"tuple_size_bytes", std::to_string(tuple_bytes.size())}
        }
    );

    DemoEventLogger::Log(
        "fsm_check",
        "Free Space Map checks for a page with enough space",
        {
            {"employee_id", std::to_string(emp.employee_id)},
            {"required_space_bytes", std::to_string(required_space)}
        }
    );

    page_id_t target_page_id = fsm_.FindPageWithEnoughSpace(required_space);

    if (target_page_id != INVALID_PAGE_ID) {
        DemoEventLogger::Log(
            "fsm_reuse",
            "FSM selected an existing page with enough free space",
            {
                {"page_id", std::to_string(target_page_id)},
                {"free_space_bytes", std::to_string(fsm_.GetFreeSpace(target_page_id))}
            }
        );
    }

    Page* page = nullptr;

    if (target_page_id == INVALID_PAGE_ID) {
        page = buffer_pool_.NewPage(&target_page_id);

        if (page == nullptr) {
            std::cout << "[INSERT ERROR] Could not allocate new page."
                      << std::endl;
            return false;
        }

        SlottedPage new_page(page->GetData());
        new_page.Init();

        fsm_.UpdateFreeSpace(target_page_id, new_page.GetFreeSpace());

        std::cout << "[FSM] No page had enough free space. Created page_id="
                  << target_page_id
                  << std::endl;
    } else {
        page = buffer_pool_.FetchPage(target_page_id);

        if (page == nullptr) {
            std::cout << "[INSERT ERROR] Could not fetch page_id="
                      << target_page_id
                      << std::endl;
            return false;
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
        std::cout << "[INSERT ERROR] FSM selected page_id="
                  << target_page_id
                  << ", but insert failed."
                  << std::endl;

        buffer_pool_.UnpinPage(target_page_id, false);
        return false;
    }

    fsm_.UpdateFreeSpace(target_page_id, slotted_page.GetFreeSpace());

    RID rid;
    rid.page_id = target_page_id;
    rid.slot_id = slot_id;

    employee_index_[emp.employee_id] = rid;
    all_rids_.push_back(rid);

    insert_count_++;

    DemoEventLogger::Log(
        "insert",
        "Tuple inserted into slotted page",
        {
            {"employee_id", std::to_string(emp.employee_id)},
            {"page_id", std::to_string(target_page_id)},
            {"slot_id", std::to_string(slot_id)},
            {"tuple_offset", std::to_string(slotted_page.GetTupleOffsetForDemo(slot_id))},
            {"tuple_size_bytes", std::to_string(tuple_bytes.size())},
            {"page_free_space_bytes", std::to_string(slotted_page.GetFreeSpace())},
            {"slot_count", std::to_string(slotted_page.GetSlotCount())},
            {"dirty", "true"}
        }
    );

    buffer_pool_.UnpinPage(target_page_id, true);

    if (print_result) {
        std::cout << "[INSERT] employee_id=" << emp.employee_id
                  << ", page_id=" << target_page_id
                  << ", slot_id=" << slot_id
                  << ", tuple_size=" << tuple_bytes.size()
                  << " bytes"
                  << ", remaining_free_space="
                  << slotted_page.GetFreeSpace()
                  << " bytes"
                  << std::endl;
    }

    ExportMetrics();
    return true;
}

void MiniDBEngine::ExplainTuple(
    int employee_id,
    const std::string& name,
    const std::string& department,
    int salary
) {
    EmployeeTuple emp;
    emp.employee_id = employee_id;
    emp.name = name;
    emp.department = department;
    emp.salary = salary;

    std::vector<char> tuple_bytes = TupleSerializer::Serialize(emp);
    std::ostringstream hex_preview;

    for (std::size_t i = 0; i < tuple_bytes.size() && i < 12; i++) {
        unsigned char byte = static_cast<unsigned char>(tuple_bytes[i]);

        hex_preview << std::hex
                    << std::uppercase
                    << std::setw(2)
                    << std::setfill('0')
                    << static_cast<int>(byte)
                    << " ";
    }

    if (tuple_bytes.size() > 12) {
        hex_preview << "...";
    }

    DemoEventLogger::Log(
        "serialize",
        "Logical row converted into serialized bytes",
        {
            {"employee_id", std::to_string(employee_id)},
            {"name", name},
            {"department", department},
            {"salary", std::to_string(salary)},
            {"tuple_size_bytes", std::to_string(tuple_bytes.size())},
            {"hex_preview", hex_preview.str()}
        }
    );

    TupleSerializer::PrintSerializationBreakdown(emp);
}

bool MiniDBEngine::GetEmployee(int employee_id) {
    auto it = employee_index_.find(employee_id);

    if (it == employee_index_.end()) {
        std::cout << "[GET] employee_id="
                  << employee_id
                  << " not found in current in-memory index."
                  << std::endl;
        return false;
    }

    RID rid = it->second;

    Page* page = buffer_pool_.FetchPage(rid.page_id);

    if (page == nullptr) {
        std::cout << "[GET ERROR] Could not fetch page_id="
                  << rid.page_id
                  << std::endl;
        ExportMetrics();
        return false;
    }

    SlottedPage slotted_page(page->GetData());
    std::vector<char> tuple_bytes;

    bool found = slotted_page.GetTuple(rid.slot_id, tuple_bytes);

    buffer_pool_.UnpinPage(rid.page_id, false);

    if (!found) {
        std::cout << "[GET ERROR] Tuple not found at page_id="
                  << rid.page_id
                  << ", slot_id=" << rid.slot_id
                  << std::endl;
        ExportMetrics();
        return false;
    }

    EmployeeTuple emp = TupleSerializer::Deserialize(
        tuple_bytes.data(),
        static_cast<int>(tuple_bytes.size())
    );

    read_count_++;

    std::cout << "[GET RESULT] "
              << "employee_id=" << emp.employee_id
              << ", name=" << emp.name
              << ", department=" << emp.department
              << ", salary=" << emp.salary
              << ", page_id=" << rid.page_id
              << ", slot_id=" << rid.slot_id
              << std::endl;

    ExportMetrics();
    return true;
}

bool MiniDBEngine::TimedGetEmployee(int employee_id) {
    auto it = employee_index_.find(employee_id);

    if (it == employee_index_.end()) {
        std::cout << "[TIMEDGET ERROR] Employee not found: "
                  << employee_id
                  << std::endl;
        return false;
    }

    RID rid = it->second;

    bool was_in_memory = buffer_pool_.ContainsPage(rid.page_id);

    auto start = std::chrono::high_resolution_clock::now();

    Page* page = buffer_pool_.FetchPage(rid.page_id);

    if (page == nullptr) {
        std::cout << "[TIMEDGET ERROR] Could not fetch page_id="
                  << rid.page_id
                  << std::endl;
        return false;
    }

    SlottedPage slotted_page(page->GetData());
    std::vector<char> tuple_bytes;

    bool found = slotted_page.GetTuple(rid.slot_id, tuple_bytes);

    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (!found) {
        buffer_pool_.UnpinPage(rid.page_id, false);

        std::cout << "[TIMEDGET ERROR] Tuple slot was deleted or invalid"
                  << std::endl;

        return false;
    }

    EmployeeTuple emp = TupleSerializer::Deserialize(
        tuple_bytes.data(),
        static_cast<int>(tuple_bytes.size())
    );

    read_count_++;

    DemoEventLogger::Log(
        "timed_fetch",
        was_in_memory ? "Memory fetch through buffer pool" : "Disk fetch through DiskManager",
        {
            {"employee_id", std::to_string(employee_id)},
            {"page_id", std::to_string(rid.page_id)},
            {"slot_id", std::to_string(rid.slot_id)},
            {"fetch_path", was_in_memory ? "BUFFER_HIT_MEMORY" : "BUFFER_MISS_DISK"},
            {"fetch_time_us", std::to_string(elapsed_us)}
        }
    );

    buffer_pool_.UnpinPage(rid.page_id, false);

    std::cout << std::endl;
    std::cout << "Timed Fetch Result" << std::endl;
    std::cout << "------------------" << std::endl;
    std::cout << "employee_id = " << emp.employee_id << std::endl;
    std::cout << "page_id     = " << rid.page_id << std::endl;
    std::cout << "slot_id     = " << rid.slot_id << std::endl;
    std::cout << "fetch path  = "
              << (was_in_memory ? "BUFFER HIT / MEMORY FETCH"
                                : "BUFFER MISS / DISK FETCH")
              << std::endl;
    std::cout << "fetch time  = "
              << elapsed_us
              << " microseconds"
              << std::endl;
    std::cout << std::endl;

    if (was_in_memory) {
        std::cout << "Meaning: page was already present in the buffer pool, so MiniDB read it from memory."
                  << std::endl;
    } else {
        std::cout << "Meaning: page was not in memory, so MiniDB had to fetch it from disk through DiskManager."
                  << std::endl;
    }

    std::cout << "Note: exact time may be affected by OS file caching, but the fetch path is real."
              << std::endl;

    ExportMetrics();
    return true;
}

bool MiniDBEngine::TouchEmployee(int employee_id) {
    auto it = employee_index_.find(employee_id);

    if (it == employee_index_.end()) {
        std::cout << "[TOUCH ERROR] Employee not found: "
                  << employee_id
                  << std::endl;
        return false;
    }

    RID rid = it->second;

    std::cout << "[TOUCH] Reading employee_id=" << employee_id
              << " from page_id=" << rid.page_id
              << ", slot_id=" << rid.slot_id
              << std::endl;

    Page* page = buffer_pool_.FetchPage(rid.page_id);

    if (page == nullptr) {
        std::cout << "[TOUCH ERROR] Could not fetch page_id="
                  << rid.page_id
                  << std::endl;
        return false;
    }

    read_count_++;

    DemoEventLogger::Log(
        "touch",
        "Page touched to update LRU-K access history",
        {
            {"employee_id", std::to_string(employee_id)},
            {"page_id", std::to_string(rid.page_id)},
            {"slot_id", std::to_string(rid.slot_id)}
        }
    );

    buffer_pool_.UnpinPage(rid.page_id, false);

    ExportMetrics();
    return true;
}

bool MiniDBEngine::DeleteEmployee(int employee_id) {
    auto it = employee_index_.find(employee_id);

    if (it == employee_index_.end()) {
        std::cout << "[DELETE ERROR] Employee not found: "
                  << employee_id
                  << std::endl;
        return false;
    }

    RID rid = it->second;

    std::cout << "[DELETE] employee_id=" << employee_id
              << " located at page_id=" << rid.page_id
              << ", slot_id=" << rid.slot_id
              << std::endl;

    Page* page = buffer_pool_.FetchPage(rid.page_id);

    if (page == nullptr) {
        std::cout << "[DELETE ERROR] Could not fetch page_id="
                  << rid.page_id
                  << std::endl;
        return false;
    }

    SlottedPage slotted_page(page->GetData());

    int deleted_tuple_size = slotted_page.GetTupleSizeForDemo(rid.slot_id);
    int free_space_before_delete = slotted_page.GetFreeSpace();

    bool deleted = slotted_page.DeleteTuple(rid.slot_id);

    if (!deleted) {
        buffer_pool_.UnpinPage(rid.page_id, false);

        std::cout << "[DELETE ERROR] Slot already deleted or invalid"
                  << std::endl;

        return false;
    }

    int free_space_after_delete = slotted_page.GetFreeSpace();

    employee_index_.erase(it);
    delete_count_++;

    DemoEventLogger::Log(
        "delete",
        "Slot marked as tombstone deleted",
        {
            {"employee_id", std::to_string(employee_id)},
            {"page_id", std::to_string(rid.page_id)},
            {"slot_id", std::to_string(rid.slot_id)},
            {"slot_status", "DELETED"},
            {"tuple_size_bytes", std::to_string(deleted_tuple_size)},
            {"page_free_space_before_bytes", std::to_string(free_space_before_delete)},
            {"page_free_space_bytes", std::to_string(free_space_after_delete)},
            {"delete_mode", "tombstone_no_compaction"}
        }
    );

    buffer_pool_.UnpinPage(rid.page_id, true);

    std::cout << "[DELETE] Slot marked as deleted." << std::endl;
    std::cout << "[DELETE] Page marked dirty because metadata changed." << std::endl;
    std::cout << "[DELETE] This is tombstone delete; physical compaction is not implemented yet."
              << std::endl;

    ExportMetrics();
    return true;
}

void MiniDBEngine::ScanEmployees() {
    scan_count_++;

    std::cout << "[SCAN] Scanning "
              << all_rids_.size()
              << " tuples from current run."
              << std::endl;

    int printed = 0;

    for (const RID& rid : all_rids_) {
        Page* page = buffer_pool_.FetchPage(rid.page_id);

        if (page == nullptr) {
            std::cout << "[SCAN ERROR] Could not fetch page_id="
                      << rid.page_id
                      << std::endl;
            continue;
        }

        SlottedPage slotted_page(page->GetData());
        std::vector<char> tuple_bytes;

        bool found = slotted_page.GetTuple(rid.slot_id, tuple_bytes);

        buffer_pool_.UnpinPage(rid.page_id, false);

        if (!found) {
            continue;
        }

        EmployeeTuple emp = TupleSerializer::Deserialize(
            tuple_bytes.data(),
            static_cast<int>(tuple_bytes.size())
        );

        if (printed < 30) {
            std::cout << "  employee_id=" << emp.employee_id
                      << ", name=" << emp.name
                      << ", department=" << emp.department
                      << ", salary=" << emp.salary
                      << ", page_id=" << rid.page_id
                      << ", slot_id=" << rid.slot_id
                      << std::endl;
        }

        printed++;
    }

    if (printed > 30) {
        std::cout << "  ... only first 30 rows shown." << std::endl;
    }

    std::cout << "[SCAN COMPLETE] rows_seen=" << printed << std::endl;

    ExportMetrics();
}

void MiniDBEngine::RunWorkload(int count, int delay_ms) {
    std::cout << "[WORKLOAD] Inserting "
              << count
              << " generated employees."
              << std::endl;

    for (int i = 0; i < count; i++) {
        EmployeeTuple emp = CreateGeneratedEmployee(next_generated_id_++);
        InsertEmployeeTuple(emp, false);

        if (delay_ms > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delay_ms)
            );
        }
    }

    std::cout << "[WORKLOAD COMPLETE] Inserted "
              << count
              << " generated employees."
              << std::endl;

    ExportMetrics();
}

EmployeeTuple MiniDBEngine::CreateGeneratedEmployee(int id) const {
    EmployeeTuple emp;
    emp.employee_id = id;
    emp.name = "Employee" + std::to_string(id);
    emp.department = "CS";
    emp.salary = 50000 + id;
    return emp;
}

void MiniDBEngine::ShowBufferPool() const {
    buffer_pool_.PrintBufferPool();
}

void MiniDBEngine::ShowFreeSpaceMap() const {
    fsm_.Print();
}

void MiniDBEngine::ShowMetrics() const {
    buffer_pool_.PrintMetrics();

    std::cout << std::endl;
    std::cout << "MiniDB Operation Counts" << std::endl;
    std::cout << "-----------------------" << std::endl;
    std::cout << "Inserts: " << insert_count_ << std::endl;
    std::cout << "Reads: " << read_count_ << std::endl;
    std::cout << "Scans: " << scan_count_ << std::endl;
    std::cout << "Deletes: " << delete_count_ << std::endl;
}

void MiniDBEngine::ShowPage(page_id_t page_id) {
    Page* page = buffer_pool_.FetchPage(page_id);

    if (page == nullptr) {
        std::cout << "[SHOW PAGE ERROR] Could not fetch page_id="
                  << page_id
                  << std::endl;
        return;
    }

    std::cout << std::endl;
    std::cout << "Page Layout for page_id=" << page_id << std::endl;
    std::cout << "-------------------------" << std::endl;

    SlottedPage slotted_page(page->GetData());
    slotted_page.PrintLayout();

    DemoEventLogger::Log(
        "show_page",
        "Slotted page layout inspected",
        {
            {"page_id", std::to_string(page_id)},
            {"slot_count", std::to_string(slotted_page.GetSlotCount())},
            {"free_space_bytes", std::to_string(slotted_page.GetFreeSpace())}
        }
    );

    buffer_pool_.UnpinPage(page_id, false);

    ExportMetrics();
}

void MiniDBEngine::ShowState() const {
    ShowBufferPool();
    std::cout << std::endl;
    ShowFreeSpaceMap();
    std::cout << std::endl;
    ShowMetrics();
}

void MiniDBEngine::ShowLRUK() const {
    buffer_pool_.PrintLRUK();
}

void MiniDBEngine::FlushAll() {
    std::cout << "[FLUSH] Flushing all dirty pages..."
              << std::endl;

    DemoEventLogger::Log(
        "flush_start",
        "Flush all dirty pages requested"
    );

    buffer_pool_.FlushAllPages();

    ExportMetrics();

    DemoEventLogger::Log(
        "flush_complete",
        "Flush all dirty pages completed"
    );

    std::cout << "[FLUSH COMPLETE]" << std::endl;
}

void MiniDBEngine::ExportMetrics() const {
    DemoEventLogger::Log(
        "metrics_export",
        "Exporting metrics",
        {
            {"insert_count", std::to_string(insert_count_)},
            {"read_count", std::to_string(read_count_)},
            {"scan_count", std::to_string(scan_count_)},
            {"delete_count", std::to_string(delete_count_)}
        }
    );
}
