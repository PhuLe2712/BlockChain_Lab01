#include "state.hpp"
namespace { std::string enc(const std::string& s){return std::to_string(s.size())+":"+s;} }
bool State::apply(const Transaction& tx,std::string* reason) {
    auto fail=[&](const std::string& why){if(reason)*reason=why;return false;};
    if(!tx.isValid()) return fail("invalid_signature_or_ownership");
    auto it=last_nonce_.find(tx.sender);
    const std::uint64_t expected=(it==last_nonce_.end()?0:it->second+1);
    if(tx.nonce!=expected) return fail("invalid_or_replayed_nonce");
    values_[tx.key]=tx.value; last_nonce_[tx.sender]=tx.nonce;
    if(reason) reason->clear();
    return true;
}
std::string State::root() const {
    std::string canonical;
    for(const auto& [k,v]:values_) canonical+=enc(k)+enc(v);
    for(const auto& [sender,nonce]:last_nonce_) canonical+=enc(sender)+std::to_string(nonce)+";";
    return crypto::sha256(canonical);
}
