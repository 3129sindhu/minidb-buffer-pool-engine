#pragma once

#include <string>
#include <unordered_map>

class DemoEventLogger {
public:
    static void Log(
        const std::string& type,
        const std::string& message,
        const std::unordered_map<std::string, std::string>& fields = {}
    );

private:
    static std::string Escape(const std::string& value);
};
