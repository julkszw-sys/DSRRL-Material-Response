#pragma once
// V301 exact-source, owner-local create-time shader candidate loader.
// Never changes the retail DSR executable, Fog, or latest_hook_source.
#include "dsrrl/runtime/hemenvlerp_v301_owner_approval.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace dsrrl::runtime::hemenvlerp_v301 {
inline bool load(std::uint32_t receiver,
    const void* original_r19,std::size_t original_size,
    std::vector<std::uint8_t>& candidate) noexcept
{
  candidate.clear();
  try {
    if(receiver<33u || receiver>35u || !original_r19 ||
       original_size<128u || approved.size()!=3u)
      return false;
    constexpr std::array<const char*,3> final_r19{{
      "1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",
      "8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",
      "b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70"
    }};
    const auto source_sha=
      operators::legacy_plan::hashing::sha256(
        static_cast<const std::uint8_t*>(original_r19),original_size);
    if(!operators::legacy_plan::hashing::matches_hex(
           source_sha,final_r19[receiver-33u]))return false;
    wchar_t module[MAX_PATH]{};
    const auto n=GetModuleFileNameW(nullptr,module,MAX_PATH);
    if(n==0u || n>=MAX_PATH)return false;
    std::filesystem::path base(module);
    const auto folder=base.parent_path() / L"DSRRL" /
                      L"EnvSpec" / L"HemEnvLerp";
    const auto file=folder /
         (L"v297_RX"+std::to_wstring(receiver)+L".dxbc");
    std::ifstream input(file,std::ios::binary|std::ios::ate);
    if(!input)return false;
    const auto length=input.tellg();
    if(length<128 || length>1024*1024)return false;
    candidate.resize(static_cast<std::size_t>(length));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(candidate.data()),length);
    if(!input)return false;
    if(!operators::legacy_plan::dxbc::checksum_container_valid(
        candidate.data(),candidate.size()))return false;
    // checksum_container_valid verifies only DXBC magic and declared size.
    // Recompute the *actual* legacy DXBC checksum into a local copy and
    // require the stored 128-bit checksum to match exactly. Fail open.
    auto checksum_copy = candidate;
    if(!operators::legacy_plan::dxbc::fix_checksum(
           checksum_copy.data(),checksum_copy.size()) ||
       !std::equal(checksum_copy.begin()+4u,checksum_copy.begin()+20u,
                   candidate.begin()+4u))
      return false;
    const auto sha=operators::legacy_plan::hashing::sha256(
        candidate.data(),candidate.size());
    bool authorized=false;
    for(const auto& row:approved){
      if(row.receiver_id==receiver && row.size==candidate.size() &&
         row.sha256 &&
         operators::legacy_plan::hashing::matches_hex(sha,row.sha256))
        authorized=true;
    }
    if(!authorized)candidate.clear();
    return authorized;
  }catch(...){candidate.clear();return false;}
}
}
