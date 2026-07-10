#include "minidb_engine.h"

#include "metrics_exporter.h"
#include "slotted_page.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <string>
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

    std::vector<char> tuple_bytes =
        TupleSerializer::Serialize(emp);
    int required_space = static_cast<int>(tuple_bytes.size()) + 8;
    page_id_t target_page_id =fsm_.FindPageWithEnoughSpace(required_space);
    Page* page = nullptr;
    if (target_page_id == INVALID_PAGE_ID) {
        page = buffer_pool_.NewPage(&target_page_id);
        if (page == nullptr) {
            std::cout << "[INSERT ERROR] Could not allocate new page."
                      << std::endl;
            return false;
        }
        SlottedPage slotted_page(page->GetData());
        slotted_page.Init();
        fsm_.UpdateFreeSpace(
            target_page_id,
            slotted_page.GetFreeSpace()
        );
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
    bool inserted = slotted_page.InsertTuple( tuple_bytes.data(),static_cast<int>(tuple_bytes.size()),slot_id
    );
    if (!inserted) {
        std::cout << "[INSERT ERROR] FSM selected page_id="
                  << target_page_id
                  << ", but insert failed."
                  << std::endl;

        buffer_pool_.UnpinPage(target_page_id, false);
        return false;
    }
    fsm_.UpdateFreeSpace(
        target_page_id,
        slotted_page.GetFreeSpace()
    );
    buffer_pool_.UnpinPage(target_page_id, true);
    RID rid;
    rid.page_id = target_page_id;
    rid.slot_id = slot_id;
    employee_index_[emp.employee_id] = rid;
    all_rids_.push_back(rid);
    insert_count_++;
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
    read_count_++;
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
    EmployeeTuple emp =
        TupleSerializer::Deserialize(
            tuple_bytes.data(),
            static_cast<int>(tuple_bytes.size())
        );
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
        EmployeeTuple emp =
            TupleSerializer::Deserialize(
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
        EmployeeTuple emp =
            CreateGeneratedEmployee(next_generated_id_++);
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
}

void MiniDBEngine::FlushAll() {
    std::cout << "[FLUSH] Flushing all dirty pages..."
              << std::endl;

    buffer_pool_.FlushAllPages();

    ExportMetrics();

    std::cout << "[FLUSH COMPLETE]" << std::endl;
}

void MiniDBEngine::ExportMetrics() const {
    MetricsExporter::ExportPrometheus(
        buffer_pool_,
        insert_count_,
        read_count_,
        scan_count_,
        metrics_file_
    );
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
    buffer_pool_.UnpinPage(page_id, false);
    ExportMetrics();
}

bool MiniDBEngine::InsertLargeEmployee(int employee_id) {
    EmployeeTuple emp;
    emp.employee_id = employee_id;
    // This large string is chosen so that it fits in an empty page,
    // This helps us force new page creation for the LRU-K demo.
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
    EmployeeTuple emp =
        TupleSerializer::Deserialize(tuple_bytes.data(),
                                     static_cast<int>(tuple_bytes.size()));
    read_count_++;
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
    bool deleted = slotted_page.DeleteTuple(rid.slot_id);
    if (!deleted) {
        buffer_pool_.UnpinPage(rid.page_id, false);

        std::cout << "[DELETE ERROR] Slot already deleted or invalid"
                  << std::endl;

        return false;
    }
    employee_index_.erase(it);
    buffer_pool_.UnpinPage(rid.page_id, true);
    std::cout << "[DELETE] Slot marked as deleted." << std::endl;
    std::cout << "[DELETE] Page marked dirty because metadata changed." << std::endl;
    std::cout << "[DELETE] This is tombstone delete; physical compaction is not implemented yet."
              << std::endl;
    ExportMetrics();
    return true;
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