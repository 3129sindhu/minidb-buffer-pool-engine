#include "minidb_engine.h"

#include <iostream>
#include <sstream>
#include <string>

void PrintHelp() {
    std::cout << std::endl;
    std::cout << "MiniDB Commands" << std::endl;
    std::cout << "---------------" << std::endl;
    std::cout << "  insert <id> <name> <department> <salary>" << std::endl;
    std::cout << "  insertbig <id>" << std::endl;
    std::cout << "  get <id>" << std::endl;
    std::cout << "  timedget <id>" << std::endl;
    std::cout << "  touch <id>" << std::endl;
    std::cout << "  delete <id>" << std::endl;
    std::cout << "  explain tuple <id> <name> <department> <salary>" << std::endl;
    std::cout << "  scan" << std::endl;
    std::cout << "  workload <count> [delay_ms]" << std::endl;
    std::cout << "  show buffer" << std::endl;
    std::cout << "  show fsm" << std::endl;
    std::cout << "  show metrics" << std::endl;
    std::cout << "  show page <page_id>" << std::endl;
    std::cout << "  show state" << std::endl;
    std::cout << "  show lruk" << std::endl;
    std::cout << "  flush" << std::endl;
    std::cout << "  help" << std::endl;
    std::cout << "  exit" << std::endl;
    std::cout << std::endl;
}

int main() {
    MiniDBEngine engine("data/minidb.db", 3, 2);

    std::cout << "MiniDB Interactive Demo" << std::endl;
    std::cout << "Type 'help' to see commands." << std::endl;
    PrintHelp();
    std::string line;
    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        if (command == "insert") {
            int id;
            std::string name;
            std::string department;
            int salary;
            if (!(iss >> id >> name >> department >> salary)) {
                std::cout << "Usage: insert <id> <name> <department> <salary>"
                          << std::endl;
                continue;
            }
            engine.InsertEmployee(id, name, department, salary);
        } else if (command == "insertbig") {
            int id;
            if (!(iss >> id)) {
                std::cout << "Usage: insertbig <id>" << std::endl;
                continue;
            }
            engine.InsertLargeEmployee(id);
        } else if (command == "get") {
            int id;
            if (!(iss >> id)) {
                std::cout << "Usage: get <id>" << std::endl;
                continue;
            }
            engine.GetEmployee(id);
        } else if (command == "timedget") {
            int id;
            if (!(iss >> id)) {
                std::cout << "Usage: timedget <id>" << std::endl;
                continue;
            }
            engine.TimedGetEmployee(id);
        } else if (command == "touch") {
            int id;
            if (!(iss >> id)) {
                std::cout << "Usage: touch <id>" << std::endl;
                continue;
            }
            engine.TouchEmployee(id);
        } else if (command == "delete") {
            int id;
            if (!(iss >> id)) {
                std::cout << "Usage: delete <id>" << std::endl;
                continue;
            }

            engine.DeleteEmployee(id);

        } else if (command == "explain") {
            std::string what;
            iss >> what;

            if (what == "tuple") {
                int id;
                std::string name;
                std::string department;
                int salary;

                if (!(iss >> id >> name >> department >> salary)) {
                    std::cout << "Usage: explain tuple <id> <name> <department> <salary>"
                              << std::endl;
                    continue;
                }

                engine.ExplainTuple(id, name, department, salary);

            } else {
                std::cout << "Usage: explain tuple <id> <name> <department> <salary>"
                          << std::endl;
            }

        } else if (command == "scan") {
            engine.ScanEmployees();

        } else if (command == "workload") {
            int count;
            int delay_ms = 0;

            if (!(iss >> count)) {
                std::cout << "Usage: workload <count> [delay_ms]"
                          << std::endl;
                continue;
            }

            iss >> delay_ms;

            engine.RunWorkload(count, delay_ms);

        } else if (command == "show") {
            std::string what;
            iss >> what;

            if (what == "buffer") {
                engine.ShowBufferPool();

            } else if (what == "fsm") {
                engine.ShowFreeSpaceMap();

            } else if (what == "metrics") {
                engine.ShowMetrics();

            } else if (what == "page") {
                page_id_t page_id;

                if (!(iss >> page_id)) {
                    std::cout << "Usage: show page <page_id>" << std::endl;
                    continue;
                }

                engine.ShowPage(page_id);

            } else if (what == "state") {
                engine.ShowState();

            } else if (what == "lruk") {
                engine.ShowLRUK();

            } else {
                std::cout << "Usage: show buffer | show fsm | show metrics | show page <page_id> | show state | show lruk"
                          << std::endl;
            }

        } else if (command == "flush") {
            engine.FlushAll();

        } else if (command == "help") {
            PrintHelp();

        } else if (command == "exit" || command == "quit") {
            engine.FlushAll();
            std::cout << "Goodbye!" << std::endl;
            break;

        } else {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Type 'help' to see commands." << std::endl;
        }
    }

    return 0;
}
