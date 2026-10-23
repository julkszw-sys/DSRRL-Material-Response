#pragma once

// Independent stock-texture SHA -> PTDE DDS replacement at D3D11 texture
// creation. Files remain in DSRRL/Diffuse and DSRRL/Normals with their
// existing names. Each existing folder may contain a sha256.map:
//   <64 hex SHA of stock DSR top-mip payload> <existing filename.dds>
// No MTD, FLVER, shader, probe, Source1, EnvSpec or draw transaction required.
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <reshade.hpp>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dsrrl::runtime::sha_texture_override {
namespace detail {
namespace fs = std::filesystem;
namespace hashing = dsrrl::operators::legacy_plan::hashing;
struct target {
    fs::path path;
    const char *kind = "";
};
inline std::once_flag initialized;
inline std::unordered_map<std::string, target> targets;
inline std::unordered_set<std::string> ambiguous;
inline thread_local std::vector<std::uint8_t> replacement_bytes;
inline std::atomic<std::uint32_t> hits{0u};
inline std::atomic<std::uint32_t> rejected{0u};

inline std::uint32_t u32(const std::vector<std::uint8_t> &b, std::size_t o) noexcept
{
    if (o > b.size() || b.size()-o < 4u) return 0u;
    return static_cast<std::uint32_t>(b[o]) |
        (static_cast<std::uint32_t>(b[o+1u]) << 8u) |
        (static_cast<std::uint32_t>(b[o+2u]) << 16u) |
        (static_cast<std::uint32_t>(b[o+3u]) << 24u);
}
inline bool lowercase_sha(std::string &sha) noexcept
{
    if (sha.size()!=64u) return false;
    for (char &c:sha) {
        if(c>='A' && c<='F') c=static_cast<char>(c-'A'+'a');
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
    }
    return true;
}
inline std::string to_hex(const hashing::sha256_digest &d)
{
    constexpr char digits[]="0123456789abcdef";
    std::string s(64u,'0');
    for(std::size_t i=0;i<32u;++i) {
        s[2u*i]=digits[d[i]>>4u]; s[2u*i+1u]=digits[d[i]&15u];
    }
    return s;
}
inline fs::path game_dir()
{
    std::array<wchar_t,32768> name{};
    const DWORD n=GetModuleFileNameW(nullptr,name.data(),static_cast<DWORD>(name.size()));
    if(n==0 || n>=name.size()) return {};
    return fs::path(std::wstring(name.data(),n)).parent_path();
}
inline void read_map(const fs::path &directory,const char *kind)
{
    std::ifstream in(directory/"sha256.map");
    if(!in) return;
    std::string line;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#') continue;
        const auto sep=line.find_first_of(" \t");
        if(sep==std::string::npos) continue;
        std::string sha=line.substr(0,sep);
        if(!lowercase_sha(sha)) continue;
        const auto begin=line.find_first_not_of(" \t",sep);
        if(begin==std::string::npos) continue;
        auto end=line.find_last_not_of(" \t\r");
        fs::path filename(line.substr(begin,end-begin+1u));
        if(filename.empty() || filename.has_parent_path() ||
           filename.extension()!=".dds") continue;
        const auto path=directory/filename;
        if(!fs::is_regular_file(path)) continue;
        if (ambiguous.count(sha)) continue;
        const auto inserted=targets.emplace(sha,target{path,kind});
        if(!inserted.second && inserted.first->second.path!=path) {
            targets.erase(inserted.first);
            ambiguous.insert(std::move(sha));
        }
    }
}
inline void setup()
{
    try {
        const fs::path root=game_dir()/"DSRRL";
        read_map(root/"Diffuse","Diffuse");
        read_map(root/"Normals","Normals");
        char msg[170]{};
        std::snprintf(msg,sizeof(msg),
            "[DSRRL SHA TEXTURE] mapping=%llu ambiguous=%llu roots=DSRRL/Diffuse,DSRRL/Normals",
            static_cast<unsigned long long>(targets.size()),
            static_cast<unsigned long long>(ambiguous.size()));
        reshade::log::message(reshade::log::level::info,msg);
    } catch(...) {
        targets.clear();
    }
}
// Format family: ignore only linear/sRGB distinction; never reinterpret
// packed channels or allow a different compression family.
inline int family(reshade::api::format f) noexcept
{
    using F=reshade::api::format;
    switch(f) {
    case F::bc1_typeless: case F::bc1_unorm: case F::bc1_unorm_srgb: return 8;
    case F::bc2_typeless: case F::bc2_unorm: case F::bc2_unorm_srgb: return 16;
    case F::bc3_typeless: case F::bc3_unorm: case F::bc3_unorm_srgb: return 17;
    case F::bc4_typeless: case F::bc4_unorm: case F::bc4_snorm: return 9;
    case F::bc5_typeless: case F::bc5_unorm: case F::bc5_snorm: return 18;
    case F::bc7_typeless: case F::bc7_unorm: case F::bc7_unorm_srgb: return 19;
    case F::r8g8b8a8_typeless: case F::r8g8b8a8_unorm:
    case F::r8g8b8a8_unorm_srgb: return 4;
    case F::b8g8r8a8_typeless: case F::b8g8r8a8_unorm:
    case F::b8g8r8a8_unorm_srgb: return 5;
    default: return 0;
    }
}
inline bool layout(int f,std::uint32_t w,std::uint32_t h,
                   std::uint32_t &pitch,std::uint32_t &rows) noexcept
{
    if(!f || !w || !h || w>16384u || h>16384u) return false;
    const std::uint32_t b=(f==8 || f==9)?8u:16u;
    if(f==4 || f==5) {pitch=w*4u;rows=h;}
    else {pitch=((w+3u)/4u)*b; rows=(h+3u)/4u;}
    return true;
}
inline int dds_family(const std::vector<std::uint8_t> &b,std::size_t &payload) noexcept
{
    if(b.size()<128u || u32(b,0)!=0x20534444u ||
       u32(b,4)!=124u || u32(b,76)!=32u ||
       (u32(b,112)&0x200u)!=0u) return 0;
    const auto four=u32(b,84);
    payload=128u;
    if(four==0x31545844u) return 8;  // DXT1
    if(four==0x33545844u) return 16; // DXT3
    if(four==0x35545844u) return 17; // DXT5
    if(four==0x31495441u || four==0x55344342u) return 9; // ATI1/BC4U
    if(four==0x32495441u || four==0x55354342u) return 18; // ATI2/BC5U
    if(four==0x30315844u) {
        if(b.size()<148u || u32(b,132)!=3u || u32(b,140)!=1u ||
           (u32(b,136)&4u)!=0u) return 0;
        payload=148u;
        switch(u32(b,128)) {
        case 71u:case 72u:return 8;
        case 74u:case 75u:return 16;
        case 77u:case 78u:return 17;
        case 80u:case 81u:return 9;
        case 83u:case 84u:return 18;
        case 98u:case 99u:return 19;
        case 28u:case 29u:return 4;
        case 87u:case 91u:return 5;
        default:return 0;
        }
    }
    if(four!=0 || u32(b,88)!=32u) return 0;
    if(u32(b,92)==0x000000ffu && u32(b,96)==0x0000ff00u &&
       u32(b,100)==0x00ff0000u && u32(b,104)==0xff000000u) return 4;
    if(u32(b,92)==0x00ff0000u && u32(b,96)==0x0000ff00u &&
       u32(b,100)==0x000000ffu && u32(b,104)==0xff000000u) return 5;
    return 0;
}
inline bool get_dds(const fs::path &path,int target_format,
    std::uint32_t width,std::uint32_t height,std::uint16_t levels,
    std::vector<std::uint8_t> &out,
    std::array<std::uint32_t,16> &offsets) noexcept
{
    try {
        const auto size=fs::file_size(path);
        if(size<128u || size>128u*1024u*1024u) return false;
        std::ifstream stream(path,std::ios::binary);
        if(!stream) return false;
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if(!stream.read(reinterpret_cast<char *>(bytes.data()),bytes.size())) return false;
        std::size_t payload=0u;
        if(dds_family(bytes,payload)!=target_format ||
           u32(bytes,16)!=width || u32(bytes,12)!=height ||
           (u32(bytes,28)?u32(bytes,28):1u)!=levels) return false;
        std::uint32_t w=width,h=height;
        const auto start=payload;
        for(std::uint32_t i=0;i<levels;++i) {
            std::uint32_t pitch=0,rows=0;
            if(!layout(target_format,w,h,pitch,rows)) return false;
            const auto sz=static_cast<std::size_t>(pitch)*rows;
            if(sz>bytes.size()-payload) return false;
            offsets[i]=static_cast<std::uint32_t>(payload-start);
            payload+=sz;
            w=std::max(1u,w/2u);h=std::max(1u,h/2u);
        }
        out.assign(bytes.begin()+start,bytes.begin()+payload);
        return true;
    } catch(...) {return false;}
}
} // namespace detail

inline bool on_create_resource(
    reshade::api::device *device,
    reshade::api::resource_desc &desc,
    reshade::api::subresource_data *data,
    reshade::api::resource_usage) noexcept
{
    using namespace detail;
    if(device==nullptr || device->get_api()!=reshade::api::device_api::d3d11 ||
       data==nullptr || desc.type!=reshade::api::resource_type::texture_2d ||
       desc.texture.depth_or_layers!=1u || desc.texture.samples!=1u ||
       desc.texture.levels==0u || desc.texture.levels>16u) return false;

    const int f=family(desc.texture.format);
    std::uint32_t pitch=0,rows=0;
    if(!layout(f,desc.texture.width,desc.texture.height,pitch,rows) ||
       data[0].data==nullptr || data[0].row_pitch<pitch) return false;

    std::call_once(initialized,setup);
    if(targets.empty()) return false;

    // Hash only actual texel bytes (without D3D11 row padding).
    hashing::detail::sha256_context digest{};
    for(std::uint32_t row=0;row<rows;++row)
        digest.update(static_cast<const std::uint8_t *>(data[0].data)+
                          static_cast<std::size_t>(row)*data[0].row_pitch,pitch);
    const std::string sha=to_hex(digest.finish());
    const auto found=targets.find(sha);
    if(found==targets.end()) return false;

    std::array<std::uint32_t,16> offsets{};
    if(!get_dds(found->second.path,f,desc.texture.width,desc.texture.height,
                desc.texture.levels,replacement_bytes,offsets)) {
        if(rejected.fetch_add(1u)<8u)
            reshade::log::message(reshade::log::level::warning,
                "[DSRRL SHA TEXTURE] found SHA, DDS incompatible; original DSR preserved");
        return false;
    }

    std::uint32_t w=desc.texture.width,h=desc.texture.height;
    for(std::uint32_t i=0;i<desc.texture.levels;++i) {
        std::uint32_t row_pitch=0,sub_rows=0;
        if(!layout(f,w,h,row_pitch,sub_rows)) return false;
        data[i].data=replacement_bytes.data()+offsets[i];
        data[i].row_pitch=row_pitch;
        data[i].slice_pitch=row_pitch*sub_rows;
        w=std::max(1u,w/2u);h=std::max(1u,h/2u);
    }
    if(hits.fetch_add(1u)<16u) {
        char msg[200]{};
        std::snprintf(msg,sizeof(msg),
            "[DSRRL SHA TEXTURE] REPLACED kind=%s sha=%.16s mips=%u",
            found->second.kind,sha.c_str(),desc.texture.levels);
        reshade::log::message(reshade::log::level::info,msg);
    }
    // ReShade D3D11 wrapper copies modified initial_data into native
    // CreateTexture2D iff callback returns true. No draw-stage SRV mutation.
    return true;
}
} // namespace dsrrl::runtime::sha_texture_override
