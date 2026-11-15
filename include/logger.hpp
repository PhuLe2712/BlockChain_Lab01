#pragma once
#include <string>
#include <cstdint>
#include <fstream>
class Logger {
public:
    explicit Logger(const std::string& path);
    void event(std::uint64_t tick,const std::string& node,const std::string& type,const std::string& detail);
private: std::ofstream out_;
};
