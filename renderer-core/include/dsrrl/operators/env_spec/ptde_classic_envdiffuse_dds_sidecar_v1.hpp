#pragma once
// Exact PTDE Classic/legacy EnvDif sidecar, wholly separate from GI_EnvDif.
// Source corpus: 10 original PTDE TPFs extracted from archived dvdbnd0/1.
// Contains 38 named map cubes from m10..m18 plus EnvDif_def_000.
// Asset-only contract, NOT authority to switch t11/t13 or bypass t12/t14.
#include <array>
#include <cstddef>
#include <cstdint>
namespace dsrrl::operators::env_spec::classic_envdiffuse_sidecar_v1 {
inline constexpr char k_relative_path[] =
    "DSRRL\\EnvDiffuse\\Legacy\\PTDE_LEGACY_ENVDIFFUSE_ALL_MAPS_DDS_V1.bin";
inline constexpr std::size_t k_file_bytes=820208u;
inline constexpr std::size_t k_header_bytes=32u;
inline constexpr std::size_t k_record_bytes=128u;
inline constexpr std::size_t k_records=39u;
inline constexpr std::size_t k_data_offset=5024u;
inline constexpr char k_magic[9]="DSRLEDF1";
inline constexpr std::array<std::uint8_t,32> k_exact_sha256={{
  0x3b,0xe0,0x5f,0xff,0xac,0xfb,0xc0,0xaf,
  0x3f,0x5a,0x41,0x24,0xf0,0xf7,0x96,0x94,
  0xc2,0x04,0x80,0x05,0x7b,0x3a,0x5c,0x76,
  0x47,0x06,0x67,0xed,0x71,0x2a,0x6b,0x47
}};
enum class dds_format : std::uint16_t { rgb24_bgr=1, bc1_dxt1=2 };
struct source_identity {
    char name[40];
    std::uint16_t map_id, legacy_row, width, height, mip_levels;
    std::uint16_t format;
    std::uint32_t cube_flag;
    std::uint64_t dds_offset, dds_bytes;
    std::array<std::uint8_t,32> dds_sha256;
    std::array<std::uint8_t,24> reserved;
};
static_assert(sizeof(source_identity)==k_record_bytes);
static_assert(k_header_bytes+k_records*k_record_bytes==k_data_offset);
// Must verify whole file SHA, per-record DDS SHA and size before any use.
// Asset lookup is by exact logical name (NOT packed GI probe index).
// Native DSR resource -> PTDE resource mapping and sampler/receiver/t12
// authorization remain independent fail-open gates until proved.
} // namespace dsrrl::operators::env_spec::classic_envdiffuse_sidecar_v1
