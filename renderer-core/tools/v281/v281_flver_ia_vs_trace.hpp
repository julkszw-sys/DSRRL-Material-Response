#pragma once
// V281: read-only exact FLVER draw-bound IA/VS snapshot.
// No shader/resource/CB/IA writes, no draw replay, no EXE disk modification.
// Only exact two V279 model SHA + four material slots and RX33/34.
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/v275_material_draw_trace.hpp"
#include <d3d11.h>
#include <reshade.hpp>
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace dsrrl::runtime::v281 {
namespace mr=dsrrl::operators::material_response;
inline constexpr const char *k_sha_a=
  "eb3b77ee419f9e2997b7d427987c002411fdf08edda12834be4a9f51b3c4eb7b";
inline constexpr const char *k_sha_b=
  "672e35606495f24e36c704f32fceda9b933563a64e03379375f47f9fe04a0a7d";
inline constexpr const char *k_metal=
  "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";
inline constexpr const char *k_leather=
  "0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e";
inline std::array<std::atomic<std::uint64_t>,1024> keys{};
inline std::atomic<std::uint32_t> overflow{0};

inline bool first_seen(std::uint64_t k) noexcept {
  k=dsrrl::runtime::v275::mix(k)|1ull;
  auto i=static_cast<unsigned>(k)&1023u;
  for(unsigned j=0;j<1024u;++j){
    auto &a=keys[(i+j)&1023u];
    auto old=a.load(std::memory_order_relaxed);
    if(old==k)return false;
    if(old==0ull && a.compare_exchange_strong(old,k,std::memory_order_relaxed))
      return true;
  }
  overflow.fetch_add(1u,std::memory_order_relaxed);
  return false;
}

inline void observe(reshade::api::command_list *cmd,
                    const mr::material_identity &m,
                    const mr::decision &d,std::uint32_t rx,bool lerp) noexcept {
  if(!cmd || !d.active || !m.valid || !m.owner_tuple_exact ||
     !m.material_slot_valid || (rx!=33u && rx!=34u))return;
  char flver[65]{},mtd[65]{};
  dsrrl::runtime::v275::hex_digest(m.flver_sha256,flver);
  dsrrl::runtime::v275::hex_digest(m.raw_mtd_sha256,mtd);
  const int asset=!std::strcmp(flver,k_sha_a)?0:!std::strcmp(flver,k_sha_b)?1:-1;
  const int mat=!std::strcmp(mtd,k_metal)?0:!std::strcmp(mtd,k_leather)?1:-1;
  if(asset<0 || mat<0)return;
  const unsigned expect_slot=asset==0?(mat==0?3u:2u):(mat==0?5u:8u);
  const unsigned expect_route=mat==0?345u:1073741869u;
  if(m.material_slot!=expect_slot || d.route_index!=expect_route)return;
  auto *ctx=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());
  if(!ctx)return;
  const auto sh=dsrrl::runtime::v274b::observe(ctx);
  unsigned bucket=0xffffu;
  if(sh.has_shadow && std::isfinite(sh.atmosphere[3]))
    bucket=static_cast<unsigned>(std::max(0.0f,
         std::min(64.0f,sh.atmosphere[3]*40.0f)));
  const std::uint64_t key=(std::uint64_t(asset)<<50)|
        (std::uint64_t(mat)<<49)|(std::uint64_t(lerp)<<48)|
        (std::uint64_t(rx)<<32)|bucket;
  if(!first_seen(key))return;

  ID3D11InputLayout*layout=nullptr;
  ID3D11VertexShader*vs=nullptr;
  ID3D11Buffer*ib=nullptr;
  ID3D11Buffer*vb[8]{};
  UINT strides[8]{},offsets[8]{},index_offset=0;
  DXGI_FORMAT index_format=DXGI_FORMAT_UNKNOWN;
  D3D11_PRIMITIVE_TOPOLOGY topology=D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  ctx->IAGetInputLayout(&layout);
  ctx->VSGetShader(&vs,nullptr,nullptr);
  ctx->IAGetVertexBuffers(0,8,vb,strides,offsets);
  ctx->IAGetIndexBuffer(&ib,&index_format,&index_offset);
  ctx->IAGetPrimitiveTopology(&topology);

  char line[1800]{};
  auto n=std::snprintf(line,sizeof(line),
    "[DSRRL V281 IA VS] asset=%d mat=%s phase=%s rx=%u slot=%u "
    "fog_bucket=%u flver=%s mtd=%s "
    "IA_LAYOUT=%016llx VS=%016llx IB=%016llx IB_FMT=%u IB_OFF=%u "
    "TOPO=%u ",
    asset,mat==0?"METAL":"LEATHER",lerp?"LERP":"STABLE",
    rx,expect_slot,bucket,flver,mtd,
    static_cast<unsigned long long>(dsrrl::runtime::v275::ptr_id(layout)),
    static_cast<unsigned long long>(dsrrl::runtime::v275::ptr_id(vs)),
    static_cast<unsigned long long>(dsrrl::runtime::v275::ptr_id(ib)),
    static_cast<unsigned>(index_format),index_offset,
    static_cast<unsigned>(topology));
  std::size_t pos=n>0?static_cast<std::size_t>(n):0;
  if(pos>=sizeof(line))pos=sizeof(line)-1;
  for(unsigned i=0;i<8u && pos<sizeof(line)-128;++i){
    D3D11_BUFFER_DESC bd{};
    if(vb[i])vb[i]->GetDesc(&bd);
    n=std::snprintf(line+pos,sizeof(line)-pos,
      "VB%u=%016llx/STR%u/OFF%u/BYTES%u/BIND%u ",
      i,static_cast<unsigned long long>(dsrrl::runtime::v275::ptr_id(vb[i])),
      strides[i],offsets[i],bd.ByteWidth,bd.BindFlags);
    if(n<0)break;
    pos+=std::min<std::size_t>(static_cast<std::size_t>(n),sizeof(line)-pos-1);
  }
  if(pos<sizeof(line)-90)
    std::snprintf(line+pos,sizeof(line)-pos,
      "IA_STATE=HOST_PRE_DRAW COM_CONTENTS=UNOBSERVED PIXEL=OPEN");
  if(ib)ib->Release();
  if(vs)vs->Release();
  if(layout)layout->Release();
  for(auto*ptr:vb)if(ptr)ptr->Release();
  reshade::log::message(reshade::log::level::info,line);
}
} // namespace dsrrl::runtime::v281
