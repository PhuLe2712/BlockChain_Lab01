#include "logger.hpp"
#include <stdexcept>
Logger::Logger(const std::string& path):out_(path,std::ios::out|std::ios::trunc){if(!out_)throw std::runtime_error("Cannot open log file: "+path);}
void Logger::event(std::uint64_t tick,const std::string& node,const std::string& type,const std::string& detail){out_<<tick<<"|"<<node<<"|"<<type<<"|"<<detail<<'\n';out_.flush();}
