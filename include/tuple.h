#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct EmployeeTuple {
    int employee_id;
    std::string name;
    std::string department;
    int salary;
};

class TupleSerializer {
public:
    static std::vector<char> Serialize(const EmployeeTuple& tuple) {
        std::vector<char> bytes;

        AppendInt(bytes, tuple.employee_id);
        AppendString(bytes, tuple.name);
        AppendString(bytes, tuple.department);
        AppendInt(bytes, tuple.salary);

        return bytes;
    }

    static EmployeeTuple Deserialize(const char* data, int length) {
        EmployeeTuple tuple;
        int offset = 0;

        tuple.employee_id = ReadInt(data, offset);
        tuple.name = ReadString(data, offset);
        tuple.department = ReadString(data, offset);
        tuple.salary = ReadInt(data, offset);

        return tuple;
    }

private:
    static void AppendInt(std::vector<char>& bytes, int value) {
        char raw[sizeof(int)];
        std::memcpy(raw, &value, sizeof(int));
        bytes.insert(bytes.end(), raw, raw + sizeof(int));
    }

    static int ReadInt(const char* data, int& offset) {
        int value;
        std::memcpy(&value, data + offset, sizeof(int));
        offset += sizeof(int);
        return value;
    }

    static void AppendString(std::vector<char>& bytes, const std::string& value) {
        uint16_t length = static_cast<uint16_t>(value.size());

        char raw_length[sizeof(uint16_t)];
        std::memcpy(raw_length, &length, sizeof(uint16_t));

        bytes.insert(bytes.end(), raw_length, raw_length + sizeof(uint16_t));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    static std::string ReadString(const char* data, int& offset) {
        uint16_t length;
        std::memcpy(&length, data + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);

        std::string value(data + offset, data + offset + length);
        offset += length;

        return value;
    }
};