#pragma once
// V277 diagnostic only. Save exact SOURCE and composed replacement DXBC pairs
// for already-authenticated Stable and Lerp P_Metal-family RX33..35 receivers.
// Does NOT mutate PS, CB, SRV, D3D state, game EXE or any renderer file.
// Game path <exe directory>\\DSRRL_V277_CAPTURE\\RX.._(stock|bridge)_HASH.dxbc
#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <vector>
#include <reshade.hpp>

namespace dsrrl::runtime::v277 {
inline std::uint64_t fnv64(const std::uint8_t *data,std::size_t n) noexcept {
    std::uint64_t h=14695981039346656037ull;
    for(std::size_t i=0u;i<n;++i){h^=data[i];h*=1099511628211ull;}
    return h;
}
inline bool store(const std::uint8_t *code,std::size_t size,
                  unsigned rx,bool lerp,bool replaced) noexcept {
    if(code==nullptr || size<64u || size>65536u ||
       rx<33u || rx>35u) return false;
    if(code[0]!='D'||code[1]!='X'||code[2]!='B'||code[3]!='C')return false;
    wchar_t game[MAX_PATH]{};
    DWORD n=GetModuleFileNameW(nullptr,game,MAX_PATH);
    if(n<3u || n>=MAX_PATH-100u) return false;
    wchar_t *slash=wcsrchr(game,L'\\');
    if(!slash)return false;
    slash[1]=L'\0';
    wchar_t dir[MAX_PATH]{};
    if(swprintf_s(dir,MAX_PATH,L"%lsDSRRL_V277_CAPTURE",game)<=0)return false;
    if(!CreateDirectoryW(dir,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
        return false;
    wchar_t file[MAX_PATH]{};
    const auto hash=fnv64(code,size);
    if(swprintf_s(file,MAX_PATH,
        L"%ls\\RX%u_%ls_%ls_%016llX.dxbc",dir,rx,
        lerp?L"LERP":L"STABLE",
        replaced?L"BRIDGE":L"STOCK",
        static_cast<unsigned long long>(hash))<=0)
        return false;
    HANDLE h=CreateFileW(file,GENERIC_WRITE,FILE_SHARE_READ,
        nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE) {
        // Repeated shader create events do not overwrite earlier captures.
        return GetLastError()==ERROR_FILE_EXISTS ||
               GetLastError()==ERROR_ALREADY_EXISTS;
    }
    DWORD written=0u;
    const BOOL ok=WriteFile(h,code,static_cast<DWORD>(size),&written,nullptr);
    CloseHandle(h);
    char line[320]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V277 EXACT DXBC CAPTURE] phase=%s rx=%u type=%s "
        "bytes=%zu fnv64=%016llx write_ok=%u "
        "source=CREATE_TIME_MATERIALIZER pixel=UNOBSERVED",
        lerp?"LERP":"STABLE",rx,
        replaced?"BRIDGE":"STOCK",size,
        static_cast<unsigned long long>(hash),
        ok&&written==size?1u:0u);
    reshade::log::message(ok&&written==size
         ? reshade::log::level::info
         : reshade::log::level::warning,line);
    return ok&&written==size;
}
inline void capture(const std::uint8_t *stock,std::size_t stock_size,
                    const std::vector<std::uint8_t> &bridge,
                    unsigned rx,bool lerp) noexcept {
    if(rx<33u || rx>35u || bridge.empty())return;
    (void)store(stock,stock_size,rx,lerp,false);
    (void)store(bridge.data(),bridge.size(),rx,lerp,true);
}
} // namespace dsrrl::runtime::v277
