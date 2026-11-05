#pragma once
#include <Windows.h>
#include <reshade.hpp>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <memory>
#include <new>
namespace dsrrl::runtime::v438 {
using u64=std::uint64_t;
enum kind:unsigned {
 WRAPPER_BEGIN=1, WRAPPER_END=2, PACKER=3, MODEL_SELECTOR=4,
 SOURCE1_JOIN=5, SOURCE1_REJECT=6, ENV_ISSUED=7, ENV_FAIL=8,
 ENV_SHA=9, FRAME=10, SESSION_END=11, HOOK_STATUS=12, MARKER=13, ENV_JOURNAL=14
};
inline u64 p(const void*x) noexcept{return static_cast<u64>(reinterpret_cast<std::uintptr_t>(x));}
inline u64 fbits(float f) noexcept{std::uint32_t bits=0;std::memcpy(&bits,&f,4);return bits;}
struct event_record {u64 seq=0,qpc=0,frame=0;unsigned tid=0,type=0;u64 value[24]{};};
struct slot {std::atomic<u64> ready{0};event_record value{};};
constexpr u64 ring_capacity=131072;
struct session {
 std::atomic<bool> active{false},finished{false},start_attempted{false};
 std::atomic<u64> head{0},tail{0},dropped{0},frames{0},started_at_ms{0},env_draws{0},marks{0};
 std::unique_ptr<slot[]> ring{}; FILE *csv=nullptr; wchar_t path[1024]{};
};
inline session& state() noexcept{static session s;return s;}
struct invocation { u64 id=0,entity=0,model=0,drawparam=0,caller=0,rawab=0,beta=0; };
inline thread_local invocation current{};
inline thread_local invocation last{};
inline std::atomic<u64> invocation_issuer{0};
inline void emit(kind t,std::initializer_list<u64> payload={}) noexcept {
 auto &s=state(); if(!s.active.load(std::memory_order_acquire))return;
 u64 head=s.head.load(std::memory_order_relaxed);
 for(;;){
  if(head-s.tail.load(std::memory_order_acquire)>=ring_capacity){s.dropped.fetch_add(1);return;}
  if(s.head.compare_exchange_weak(head,head+1,std::memory_order_acq_rel,std::memory_order_relaxed))break;
 }
 LARGE_INTEGER q{};QueryPerformanceCounter(&q);
 event_record record{};record.seq=head+1;record.qpc=static_cast<u64>(q.QuadPart);
 record.frame=s.frames.load();record.tid=GetCurrentThreadId();record.type=static_cast<unsigned>(t);
 unsigned index=0;for(auto v:payload){if(index>=24u)break;record.value[index++]=v;}
 slot &dst=s.ring[head%ring_capacity];dst.value=record;
 dst.ready.store(head+1,std::memory_order_release);
}
inline void drain(unsigned budget) noexcept {
 auto &s=state(); if(!s.csv)return;
 for(unsigned i=0;i<budget;++i){
  const u64 tail=s.tail.load(std::memory_order_relaxed);
  if(tail>=s.head.load(std::memory_order_acquire))break;
  slot &src=s.ring[tail%ring_capacity];
  if(src.ready.load(std::memory_order_acquire)!=tail+1)break;
  const event_record &r=src.value;
  std::fprintf(s.csv,"%llu,%llu,%llu,%u,%u",
   static_cast<unsigned long long>(r.seq),static_cast<unsigned long long>(r.qpc),
   static_cast<unsigned long long>(r.frame),r.tid,r.type);
  for(const auto v:r.value)std::fprintf(s.csv,",%016llx",static_cast<unsigned long long>(v));
  std::fputc('\n',s.csv);src.ready.store(0,std::memory_order_release);
  s.tail.store(tail+1,std::memory_order_release);
 }
}
inline void start_one_shot() noexcept {
 auto &s=state();if(s.start_attempted.exchange(true)||s.finished.load())return;
 s.ring.reset(new(std::nothrow) slot[ring_capacity]);
 if(!s.ring){reshade::log::message(reshade::log::level::error,
    "[DSRRL V438] CAPTURE_FATAL no_memory");s.finished.store(true);return;}
 wchar_t temp[1024]{};auto n=GetTempPathW(1024,temp);
 if(n==0||n>=900){reshade::log::message(reshade::log::level::error,
    "[DSRRL V438] CAPTURE_FATAL temp_folder");s.finished.store(true);return;}
 wchar_t folder[1024]{};swprintf_s(folder,1024,L"%lsDSRRL_V438",temp);
 if(!CreateDirectoryW(folder,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS){
    reshade::log::message(reshade::log::level::error,"[DSRRL V438] CAPTURE_FATAL create_folder");
    s.finished.store(true);return;
 }
 SYSTEMTIME now{};GetLocalTime(&now);
 swprintf_s(s.path,1024,L"%ls\\V438_%04u%02u%02u_%02u%02u%02u.csv",folder,
    now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
 if(_wfopen_s(&s.csv,s.path,L"wb")!=0||!s.csv){
    reshade::log::message(reshade::log::level::error,"[DSRRL V438] CAPTURE_FATAL open_csv");
    s.finished.store(true);return;
 }
 setvbuf(s.csv,nullptr,_IOFBF,1u<<20);
 LARGE_INTEGER hz{};QueryPerformanceFrequency(&hz);
 std::fprintf(s.csv,"# V438_one_shot_v1,qpc_hz=%lld,source_head=V437,scope=DSR_diagnostic_only\n",
    static_cast<long long>(hz.QuadPart));
 std::fprintf(s.csv,"seq,qpc,frame,tid,kind");for(unsigned i=0;i<24;++i)std::fprintf(s.csv,",v%02u",i);
 std::fputc('\n',s.csv);
 s.started_at_ms.store(GetTickCount64());s.active.store(true,std::memory_order_release);
 reshade::log::message(reshade::log::level::warning,
    "[DSRRL V438] CAPTURE_START F9: 180s. Mark flicker F10. File TEMP\\DSRRL_V438\\V438_*.csv");
}
inline void on_present() noexcept {
 auto &s=state();const u64 frame=s.frames.fetch_add(1)+1;
 static bool was_f9=false;
 const bool now_f9=(GetAsyncKeyState(VK_F9)&0x8000)!=0;
 if(now_f9 && !was_f9 && !s.start_attempted.load())start_one_shot();
 was_f9=now_f9;
 if(!s.active.load())return;
 static bool was_f10=false;
 const bool now_f10=(GetAsyncKeyState(VK_F10)&0x8000)!=0;
 if(now_f10 && !was_f10){
   const auto marker=s.marks.fetch_add(1u)+1u;
   emit(MARKER,{marker,frame,GetTickCount64()});
   reshade::log::message(reshade::log::level::warning,
       "[DSRRL V438] FLICKER_MARKER captured F10");
 }
 was_f10=now_f10;
 if(frame%60u==0u)emit(FRAME,{frame,GetTickCount64(),s.env_draws.load()});
 drain(12000);
 if(frame%60u==0u && s.csv)std::fflush(s.csv);
 if(GetTickCount64()-s.started_at_ms.load()<180000u)return;
 emit(SESSION_END,{frame,s.head.load(),s.dropped.load(),s.env_draws.load(),s.marks.load()});
 s.active.store(false,std::memory_order_release);
 for(unsigned tries=0;tries<200u&&s.tail.load()!=s.head.load();++tries){drain(16000);Sleep(1);}
 if(s.csv){
  std::fprintf(s.csv,"# complete=%u,events=%llu,dropped=%llu,env_draws=%llu,markers=%llu\n",
   s.tail.load()==s.head.load()?1u:0u,
   static_cast<unsigned long long>(s.tail.load()),
   static_cast<unsigned long long>(s.dropped.load()),
   static_cast<unsigned long long>(s.env_draws.load()),
   static_cast<unsigned long long>(s.marks.load()));
  std::fclose(s.csv);s.csv=nullptr;
 }
 s.finished.store(true);
 reshade::log::message(reshade::log::level::warning,
    "[DSRRL V438] CAPTURE_END. Submit CSV from TEMP\\DSRRL_V438 and ReShade.log; pixel remains OPEN.");
}
}