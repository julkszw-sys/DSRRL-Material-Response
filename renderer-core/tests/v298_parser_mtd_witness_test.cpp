#include "dsrrl/runtime/v298_parser_mtd_witness.hpp"
#include <cassert>
#include <cstdint>

int main() {
    using namespace dsrrl::runtime::v298;
    parser_mtd_cache cache;
    parser_mtd_entry out{};
    auto *material=reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x16000u));
    dsrrl::core::sha256_digest original{};
    original[0]=0x12u; original[31]=0xafu;
    assert(!cache.lookup(material,out));
    cache.publish(material,original,1024u);
    assert(cache.lookup(material,out));
    assert(out.material==material && out.raw==original && out.raw_bytes==1024u);
    // Retail MTD object recycling must not leave a stale source identity.
    cache.erase(material);
    assert(!cache.lookup(material,out));
    dsrrl::core::sha256_digest modified{};
    modified[0]=0x39u;
    cache.publish(material,modified,2048u);
    assert(cache.lookup(material,out) && out.raw==modified);
    assert(out.raw!=original);
    cache.publish(nullptr,original,64u);
    assert(!cache.lookup(nullptr,out));
    cache.erase(material);
    assert(!cache.lookup(material,out));
}
