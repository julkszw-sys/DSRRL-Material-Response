#include "dsrrl/operators/env_spec/native_envdiffuse_identity.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace env = dsrrl::operators::env_spec;
using bytes = std::array<std::uint8_t,env::k_native_envdiffuse_atlas_size>;
void put32(bytes &b,std::size_t p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i) b[p+i]=static_cast<std::uint8_t>(v>>(i*8));
}
int main() {
    bytes b{};
    std::memcpy(b.data(),"DSREND01",8);
    put32(b,8,1);put32(b,12,342);put32(b,16,36);put32(b,20,0);
    for(std::size_t i=0;i<342;++i) {
        const std::size_t pos=24+i*36;
        b[pos]=static_cast<std::uint8_t>(i);
        b[pos+1]=static_cast<std::uint8_t>(i>>8);
        b[pos+32]=static_cast<std::uint8_t>(i);
        b[pos+33]=static_cast<std::uint8_t>(i>>8);
    }
    env::native_diffuse_hash_atlas atlas{};
    assert(env::parse_native_diffuse_hash_atlas(b.data(),b.size(),atlas));
    assert(env::unique_native_diffuse_probe(atlas,atlas[188].sha)==188);
    assert(env::unique_native_diffuse_probe(atlas,atlas[200].sha)==200);
    assert(!env::unique_native_diffuse_probe(atlas,{}).has_value());
    atlas[322].sha=atlas[321].sha;
    assert(!env::unique_native_diffuse_probe(atlas,atlas[321].sha).has_value());
    assert(env::unique_native_diffuse_probe(atlas,atlas[188].sha)==188);
    b[24+322*36+32]=0; // non-contiguous ordinal and duplicate ordinal
    assert(!env::parse_native_diffuse_hash_atlas(b.data(),b.size(),atlas));
    b[24+322*36+32]=static_cast<std::uint8_t>(322);
    assert(env::parse_native_diffuse_hash_atlas(b.data(),b.size(),atlas));
    put32(b,12,343); // header count mismatch
    assert(!env::parse_native_diffuse_hash_atlas(b.data(),b.size(),atlas));
    put32(b,12,342);
    b[0]='X'; // invalid magic
    assert(!env::parse_native_diffuse_hash_atlas(b.data(),b.size(),atlas));
    std::cout<<"V359 independent EnvDiffuse hash identity PASS\n";
}
