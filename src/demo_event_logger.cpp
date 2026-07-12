#include "demo_event_logger.h"

#include <chrono>
#include <fstream>

std::string DemoEventLogger::Escape(const std::string& value) {
    std::string escaped;

    for (char c : value) {
        if (c == '"') {
            escaped += "\\\"";
        } else if (c == '\\') {
            escaped += "\\\\";
        } else if (c == '\n') {
            escaped += "\\n";
        } else if (c == '\r') {
            escaped += "\\r";
        } else {
            escaped += c;
        }
    }

    return escaped;
}

void DemoEventLogger::Log(
    const std::string& type,
    const std::string& message,
    const std::unordered_map<std::string, std::string>& fields
) {
    std::ofstream out("data/events.jsonl", std::ios::app);

    if (!out.is_open()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    out << "{";
    out << "\"ts_ms\":" << ms << ",";
    out << "\"type\":\"" << Escape(type) << "\",";
    out << "\"message\":\"" << Escape(message) << "\"";

    for (const auto& entry : fields) {
        out << ",\"" << Escape(entry.first) << "\":"
            << "\"" << Escape(entry.second) << "\"";
    }

    out << "}\n";
}
