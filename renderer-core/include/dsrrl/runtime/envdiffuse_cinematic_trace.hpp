#pragma once
#include <atomic>
#include <cstdint>
namespace dsrrl::runtime::envdiffuse_cinematic_trace {
struct witness { std::uint64_t id=0,frame=0,serial=0,sem=0,flver=0,bank_a=0,bank_b=0; std::uint32_t row_a=0,row_b=0,rx=0,route=0,slot=0,owner=0;bool source=false,ready=false,selected=false; };
inline std::atomic<std::uint64_t> frame{0},observed{0},ready_count{0},native_ok{0},native_fail{0},prepare_fail{0};
inline thread_local witness local{};
inline void reset() noexcept {local={};}
inline void begin(std::uint64_t a,std::uint64_t b,std::uint64_t serial,std::uint32_t rowa,std::uint32_t rowb,std::uint32_t rx,std::uint32_t route,std::uint64_t sem,std::uint64_t flver,std::uint32_t slot,bool owner) noexcept {
 local={};if(a!=0xc9b879a8c5e624d0ull&&b!=0xc9b879a8c5e624d0ull&&a!=0xe447e0e74af8d1d3ull&&b!=0xe447e0e74af8d1d3ull)return;
 const auto n=observed.fetch_add(1u,std::memory_order_relaxed)+1u;
 local.id=n;local.frame=frame.load(std::memory_order_relaxed);local.serial=serial;local.bank_a=a;local.bank_b=b;local.row_a=rowa;local.row_b=rowb;local.rx=rx;local.route=route;local.sem=sem;local.flver=flver;local.slot=slot;local.owner=owner?1u:0u;local.source=true;local.selected=n<=1536u||(n<=1048576u&&(n%128u)==0u);
}
}
