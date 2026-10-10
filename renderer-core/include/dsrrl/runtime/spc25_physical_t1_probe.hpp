#pragma once
// Opt-in SPC25 passive exact native resource provenance, no GPU mutation.
#include "dsrrl/runtime/stock_dsr_compressed_resource_identity.hpp"
#include "dsrrl/runtime/spc25_stock_source_manifest.hpp"
#include <reshade.hpp>
#include <d3d11.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dsrrl::runtime::spc25_physical {
struct record {
 ID3D11Device *device=nullptr;
 std::uint32_t width=0,height=0,mips=0,format=0;
 stock_dsr_compressed_identity::digest sha{};
 const char *source=nullptr;
 const char *stage="NO_INITIAL_DATA";
};
inline std::mutex mu;
inline std::unordered_map<std::uint64_t,record> resources;
inline std::unordered_set<std::uint64_t> ambiguous;
inline std::unordered_map<std::uint64_t,std::uint64_t> view_parents;
inline std::unordered_set<std::uint64_t> conflicting_views;
inline std::unordered_set<std::uint64_t> reported_views;
inline std::atomic<std::uint32_t> reports{0u};
inline std::atomic<std::uint64_t> total_hashed{0u};
inline std::atomic<std::uint64_t> budget_bytes{0u};

inline void init(reshade::api::device *dev,
 const reshade::api::resource_desc &d,
 const reshade::api::subresource_data *src,
 reshade::api::resource r) noexcept {
 if(!dev || !r.handle ||
    d.type!=reshade::api::resource_type::texture_2d)return;
 const auto fmt=static_cast<std::uint32_t>(d.texture.format);
 if(fmt!=71u && fmt!=77u && fmt!=83u)return;
 const auto &t=d.texture;
 if(t.levels==0 || t.levels>16 || t.depth_or_layers!=1 ||
    t.samples!=1 || t.width==0 || t.height==0 ||
    t.width>4096 || t.height>4096)return;
 record rec{};
 rec.device=reinterpret_cast<ID3D11Device *>(dev->get_native());
 rec.width=t.width;rec.height=t.height;rec.mips=t.levels;rec.format=fmt;
 if(src!=nullptr) {
   std::array<stock_dsr_compressed_identity::mip_initial_data,16> mips{};
   std::uint64_t bytes=0;
   const auto block=fmt==71u?8u:16u;
   for(std::uint32_t i=0;i<t.levels;i++) {
     mips[i]={static_cast<const std::uint8_t *>(src[i].data),
              src[i].row_pitch,src[i].slice_pitch};
     const auto w=(t.width>>i)?(t.width>>i):1u;
     const auto h=(t.height>>i)?(t.height>>i):1u;
     bytes+=static_cast<std::uint64_t>((w+3u)/4u)*((h+3u)/4u)*block;
   }
   if(bytes>32u*1024u*1024u ||
      budget_bytes.fetch_add(bytes)>384u*1024u*1024u) {
     rec.stage="BUDGET_EXCEEDED";
   } else {
     const stock_dsr_compressed_identity::texture2d_descriptor desc{
       t.width,t.height,t.levels,1u,1u,fmt,true};
     const auto sha=stock_dsr_compressed_identity::exact_full_mip_digest(
       desc,mips.data(),t.levels);
     if(sha) {
       rec.sha=*sha;rec.stage="FULL_INITIAL_MIPS";
       total_hashed.fetch_add(1u);
       const auto *match=spc25_stock_source_manifest::unique_exact(
         t.width,t.height,t.levels,fmt,*sha);
       if(match)rec.source=match->logical;
     } else rec.stage="PARTIAL_OR_UNSUPPORTED_UPLOAD";
   }
 }
 try {
   std::lock_guard<std::mutex> lock(mu);
   if(ambiguous.count(r.handle))return;
   const auto hit=resources.emplace(r.handle,rec);
   if(!hit.second) {
     resources.erase(hit.first);
     ambiguous.insert(r.handle);
   }
 } catch(...) { }
}
inline void drop_resource(std::uint64_t r) noexcept {
 std::lock_guard<std::mutex> lock(mu);
 resources.erase(r);ambiguous.erase(r);
 for(auto it=view_parents.begin();it!=view_parents.end();)
   if(it->second==r)it=view_parents.erase(it);
   else ++it;
}
inline void link_view(std::uint64_t v,std::uint64_t r) noexcept {
 if(!v || !r)return;
 try {
  std::lock_guard<std::mutex> lock(mu);
  if(conflicting_views.count(v))return;
  const auto item=view_parents.emplace(v,r);
  if(!item.second && item.first->second!=r) {
    view_parents.erase(item.first);
    conflicting_views.insert(v);
  }
 } catch(...) { }
}
inline void drop_view(std::uint64_t v) noexcept {
 std::lock_guard<std::mutex> lock(mu);
 reported_views.erase(v);
 view_parents.erase(v);
 conflicting_views.erase(v);
}
inline void drop_device(ID3D11Device *d) noexcept {
 std::lock_guard<std::mutex> lock(mu);
 for(auto it=resources.begin();it!=resources.end();)
   if(it->second.device==d)it=resources.erase(it);
   else ++it;
 ambiguous.clear();reported_views.clear();view_parents.clear();conflicting_views.clear();
}
inline void clear() noexcept {
 std::lock_guard<std::mutex> lock(mu);
 resources.clear();ambiguous.clear();reported_views.clear();view_parents.clear();conflicting_views.clear();
}

inline void inspect(ID3D11ShaderResourceView *view) noexcept {
 if(!view)return;
 ID3D11Resource *r=nullptr;
 view->GetResource(&r);
 if(!r)return;
 ID3D11Device *d=nullptr;
 view->GetDevice(&d);
 const auto rid=static_cast<std::uint64_t>(
   reinterpret_cast<std::uintptr_t>(r));
 const auto vid=static_cast<std::uint64_t>(
   reinterpret_cast<std::uintptr_t>(view));
 record rec{};bool found=false,collision=false,log=false;
 {
  std::lock_guard<std::mutex> lock(mu);
  const auto it=resources.find(rid);
  if(it!=resources.end() && it->second.device==d) {
   rec=it->second;found=true;
  }
  collision=ambiguous.count(rid)!=0;
  if(reports.load()<48u && reported_views.insert(vid).second) {
   ++reports;log=true;
  }
 }
 if(log) {
  D3D11_RESOURCE_DIMENSION kind=D3D11_RESOURCE_DIMENSION_UNKNOWN;
  r->GetType(&kind);
  char hex[65]{};
  if(found && rec.stage[0]=='F') {
   const char *digits="0123456789abcdef";
   for(std::size_t i=0;i<32;++i) {
    hex[i*2]=digits[rec.sha[i]>>4];
    hex[i*2+1]=digits[rec.sha[i]&15u];
   }
  }
  char msg[720]{};
  std::snprintf(msg,sizeof(msg),
   "[DSRRL SPC25 PHYSICAL T1] stage=%s view=0x%llx native_resource=0x%llx native_type=%u create_seen=%u ambiguous=%u stock_initial_sha256=%s source_candidate=%s source_only=1 current_gpu_bytes=UNVERIFIED srv_swap=0 pixel=OPEN",
   found?rec.stage:"NO_CREATE_RECORD",
   static_cast<unsigned long long>(vid),
   static_cast<unsigned long long>(rid),
   static_cast<unsigned>(kind),found?1u:0u,collision?1u:0u,
   hex[0]?hex:"NONE",
   (!collision && rec.source)?rec.source:"NONE");
  reshade::log::message(reshade::log::level::info,msg);
 }
 if(d)d->Release();
 r->Release();
}
} // namespace dsrrl::runtime::spc25_physical
