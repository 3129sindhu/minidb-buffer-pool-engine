#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iomanip>
#include <iostream>
#include <sstream>


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

    static void PrintSerializationBreakdown(const EmployeeTuple& tuple) {
    std::vector<char> bytes = Serialize(tuple);

    std::cout << std::endl;
    std::cout << "Tuple Serialization Breakdown" << std::endl;
    std::cout << "-----------------------------" << std::endl;

    std::cout << "Logical row:" << std::endl;
    std::cout << "  employee_id = " << tuple.employee_id << std::endl;
    std::cout << "  name        = " << tuple.name << std::endl;

std::cout << "  department  = " << tuple.department << std::endl;
    std::cout << "  salary      = " << tuple.salary << std::endl;
    std::cout << std::endl;
    std::cout << "Field-wise conversion:" << std::endl;
    std::cout << "  employee_id INT      -> 4 bytes" << std::endl;
    std::cout << "  name length          -> 2 bytes" << std::endl;

    std::cout << "  name data            -> " << tuple.name.size() << " bytes" << std::endl;
    std::cout << "  department length    -> 2 bytes" << std::endl;
    std::cout << "  department data      -> " << tuple.department.size() << " bytes" << std::endl;

    std::cout << "  salary INT           -> 4 bytes" << std::endl;
    std::cout << std::endl;
    std::cout << "Total serialized tuple size: "
              << bytes.size()
              << " bytes"
              << std::endl;
    std::cout << std::endl;
    std::cout << "Serialized bytes in hex:" << std::endl;
    std::cout << "  ";

    for (unsigned char byte : bytes) {
        std::cout << std::hex
                  << std::uppercase
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int>(byte)
                  << " ";
    }

    std::cout << std::dec << std::endl;
    std::cout << std::endl;
    std::cout << "Meaning:" << std::endl;
    std::cout << "  The row is no longer stored as text." << std::endl;
    std::cout << "  It is now a compact byte array that can be placed inside a database page." << std::endl;
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