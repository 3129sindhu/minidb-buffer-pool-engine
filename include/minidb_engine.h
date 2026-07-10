#pragma once

#include "buffer_pool_manager.h"
#include "disk_manager.h"
#include "free_space_map.h"
#include "rid.h"
#include "tuple.h"

#include <string>
#include <unordered_map>
#include <vector>

class MiniDBEngine {
public:
    MiniDBEngine(
        const std::string& db_file,
        int buffer_pool_size = 3,
        int replacer_k = 2
    );
    bool InsertEmployee(
        int employee_id,
        const std::string& name,
        const std::string& department,
        int salary
    );
    bool GetEmployee(int employee_id);
    void ScanEmployees();
    void RunWorkload(int count, int delay_ms);
    void ShowBufferPool() const;
    void ShowFreeSpaceMap() const;
    void ShowMetrics() const;
    bool InsertLargeEmployee(int employee_id);
bool TouchEmployee(int employee_id);
bool TimedGetEmployee(int employee_id);
bool DeleteEmployee(int employee_id);
void ShowLRUK() const;
    void FlushAll();
    void ExportMetrics() const;
    void ShowPage(page_id_t page_id);
void ShowState() const;
void ExplainTuple(
    int employee_id,
    const std::string& name,
    const std::string& department,
    int salary
);
private:
    bool InsertEmployeeTuple(const EmployeeTuple& emp, bool print_result);
    EmployeeTuple CreateGeneratedEmployee(int id) const;
    DiskManager disk_;
    BufferPoolManager buffer_pool_;
    FreeSpaceMap fsm_;
std::unordered_map<int, RID> employee_index_;

    std::vector<RID> all_rids_;
long insert_count_;

    long read_count_;
    long scan_count_;

    int next_generated_id_;
    std::string metrics_file_;
};