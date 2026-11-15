#include "transaction.hpp"
#include <stdexcept>
namespace { std::string field(const std::string& s){ return std::to_string(s.size())+":"+s; } }
std::string Transaction::payload() const { return field(sender)+field(key)+field(value)+std::to_string(nonce); }
void Transaction::sign(const crypto::KeyPair& signer) {
    if(sender.empty()) sender=signer.address();
    if(sender!=signer.address()) throw std::invalid_argument("Signer does not own sender address");
    signature=crypto::sign(signer,"TX:lab01",payload());
}
bool Transaction::isValid() const {
    if(sender.empty()||key.empty()||signature.empty()) return false;
    // A participant may modify only keys in their own namespace.
    if(key.rfind(sender+"/",0)!=0) return false;
    return crypto::verify(sender,"TX:lab01",payload(),signature);
}
std::string Transaction::id() const { return crypto::sha256(payload()+signature); }
