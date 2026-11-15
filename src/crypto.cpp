#include "crypto.hpp"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <memory>

namespace crypto {
namespace {
using PKey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using MdCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
std::string signedPayload(const std::string& domain, const std::string& message) {
    return domain + ":" + std::to_string(message.size()) + ":" + message;
}
}
std::string toHex(const std::vector<unsigned char>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out; out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) { out.push_back(digits[b >> 4]); out.push_back(digits[b & 15]); }
    return out;
}
std::vector<unsigned char> fromHex(const std::string& hex) {
    if (hex.size() % 2) throw std::invalid_argument("Hex string has odd length");
    auto nibble=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; };
    std::vector<unsigned char> out; out.reserve(hex.size()/2);
    for(size_t i=0;i<hex.size();i+=2){ int a=nibble(hex[i]), b=nibble(hex[i+1]); if(a<0||b<0) throw std::invalid_argument("Invalid hex"); out.push_back(static_cast<unsigned char>((a<<4)|b)); }
    return out;
}
std::string KeyPair::address() const { return toHex(public_key); }
KeyPair keyPairFromSeed(const std::vector<unsigned char>& seed) {
    if(seed.size()!=32) throw std::invalid_argument("Ed25519 seed must contain 32 bytes");
    PKey p(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,seed.data(),seed.size()), EVP_PKEY_free);
    if(!p) throw std::runtime_error("Cannot create Ed25519 private key");
    std::vector<unsigned char> pub(32); size_t len=pub.size();
    if(EVP_PKEY_get_raw_public_key(p.get(),pub.data(),&len)!=1) throw std::runtime_error("Cannot derive public key");
    return {seed,pub};
}
std::string sha256(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    return toHex(std::vector<unsigned char>(digest,digest+SHA256_DIGEST_LENGTH));
}
std::string sign(const KeyPair& key, const std::string& domain, const std::string& message) {
    PKey p(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,key.private_key.data(),key.private_key.size()),EVP_PKEY_free);
    if(!p) throw std::runtime_error("Invalid signing key");
    auto payload=signedPayload(domain,message); MdCtx ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    if(!ctx || EVP_DigestSignInit(ctx.get(),nullptr,nullptr,nullptr,p.get())!=1) throw std::runtime_error("Ed25519 sign init failed");
    size_t len=0; if(EVP_DigestSign(ctx.get(),nullptr,&len,reinterpret_cast<const unsigned char*>(payload.data()),payload.size())!=1) throw std::runtime_error("Signature sizing failed");
    std::vector<unsigned char> sig(len);
    if(EVP_DigestSign(ctx.get(),sig.data(),&len,reinterpret_cast<const unsigned char*>(payload.data()),payload.size())!=1) throw std::runtime_error("Signing failed");
    sig.resize(len); return toHex(sig);
}
bool verify(const std::string& public_key_hex,const std::string& domain,const std::string& message,const std::string& signature_hex) {
    try {
        auto pub=fromHex(public_key_hex), sig=fromHex(signature_hex);
        if(pub.size()!=32 || sig.size()!=64) return false;
        PKey p(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,pub.data(),pub.size()),EVP_PKEY_free);
        MdCtx ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free); if(!p||!ctx) return false;
        if(EVP_DigestVerifyInit(ctx.get(),nullptr,nullptr,nullptr,p.get())!=1) return false;
        auto payload=signedPayload(domain,message);
        return EVP_DigestVerify(ctx.get(),sig.data(),sig.size(),reinterpret_cast<const unsigned char*>(payload.data()),payload.size())==1;
    } catch(...) { return false; }
}
}
