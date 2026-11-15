#pragma once
#include "transaction.hpp"
#include <map>
#include <string>
class State {
public:
    bool apply(const Transaction& tx, std::string* reason=nullptr);
    std::string root() const;
    const std::map<std::string,std::string>& data() const { return values_; }
private:
    std::map<std::string,std::string> values_;
    std::map<std::string,std::uint64_t> last_nonce_;
};
