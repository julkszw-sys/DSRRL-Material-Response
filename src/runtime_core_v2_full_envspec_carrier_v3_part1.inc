#define NOMINMAX
#include "runtime_core_v2_reshade.hpp"
#include "generated/envspec_receivers.hpp"

#include <reshade.hpp>
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace dsrrl::runtime_v2::full_envspec_v3 {
namespace {

using namespace dsrrl::full_envspec_generated;

enum class material_envspec_state : std::uint8_t { unknown=0, present=1, explicit_none=2, nospc_host=3 };
struct material_record {
    std::uint64_t name_hash = 0;
    std::array<std::uint8_t,32> raw_sha{};
    material_envspec_state state = material_envspec_state::unknown;
    std::uint8_t slot = 0;
    bool explicit_none_safe = false;
    float c101[3]{};
    float c102 = 0.0f;
    float c100[3]{};
};
struct native_probe_hash_record {
    std::array<std::uint8_t,32> sha{};
    std::uint16_t probe_index = 0;
};

constexpr std::uintptr_t k_parser_rva = 0x295ED0;
constexpr std::uintptr_t k_selector_rva = 0x22BA20;
constexpr std::size_t k_detour_stolen = 15;
constexpr std::uint32_t k_probe_count = 342;
constexpr std::uint32_t k_ptde_slots = 4;
constexpr std::uint32_t k_ptde_size = 32;
constexpr std::uint32_t k_faces = 6;
constexpr std::uint32_t k_ptde_bytes_per_cube = k_ptde_size * k_ptde_size * 4u * k_faces;
constexpr std::uint32_t k_dsr_size = 256;
constexpr std::uint32_t k_dsr_levels = 8;
constexpr std::uint32_t k_dsr_subresources = k_faces * k_dsr_levels;
constexpr std::size_t k_dsr_tight_bc6h_bytes = 524352;

constexpr char k_expected_exe_sha256[] =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr char k_expected_pack_sha256[] =
    "c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3";
constexpr char k_expected_material_router_sha256[] =
    "0701154f5fa683aae95094a65bad32320baa74a3f8855183b750fd7383d28b8a";
constexpr char k_expected_native_probe_sha256[] =
    "ba996b8b6c4851466ff2106a1f01f17e2711aca1a4fba85efc87873123663cbc";

constexpr std::array<std::uint8_t, k_detour_stolen> k_parser_prolog = {
    0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF};
constexpr std::array<std::uint8_t, k_detour_stolen> k_selector_prolog = {
    0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA};

struct detour_state {
    void *target = nullptr;
    void *trampoline = nullptr;
    std::array<std::uint8_t, k_detour_stolen> original{};
    bool installed = false;
};

struct material_binding {
    const material_record *record = nullptr;
    bool explicit_none_safe = false;
};

struct virtual_cube {
    reshade::api::resource resource{};
    reshade::api::resource_view view{};
    std::uint16_t probe_index = 0;
    std::uint8_t slot = 0;
    bool black = false;
};

struct srv_binding {
    bool valid = false;
    reshade::api::pipeline_layout layout{};
    std::uint32_t param_index = 0;
    reshade::api::descriptor_type type = reshade::api::descriptor_type::shader_resource_view;
    reshade::api::resource_view view{};
    reshade::api::sampler sampler{};
};

struct sampler_binding {
    bool valid = false;
    reshade::api::pipeline_layout layout{};
    std::uint32_t param_index = 0;
    reshade::api::sampler sampler{};
};

struct command_bindings {
    std::array<srv_binding, 2> srv{};
    std::array<sampler_binding, 2> sampler{};
};

struct native_resource_record {
    std::uint16_t probe_index = 0;
};

std::mutex g_mutex;
std::unordered_map<std::uintptr_t, material_binding> g_materials;
std::unordered_map<std::uint64_t, native_resource_record> g_native_resources;
std::unordered_map<std::uint64_t, std::uint64_t> g_source_resource_by_view;
std::unordered_map<std::uint64_t, command_bindings> g_command_bindings;
std::unordered_map<std::uint32_t, virtual_cube> g_virtual_cubes;

std::vector<std::uint8_t> g_ptde_pack;
std::vector<material_record> g_material_registry;
std::vector<native_probe_hash_record> g_native_probe_registry;
reshade::api::device *g_device = nullptr;
reshade::api::sampler g_ptde_sampler{};
reshade::api::resource g_black_resource{};
reshade::api::resource_view g_black_view{};

detour_state g_parser_detour;
detour_state g_selector_detour;

thread_local std::uintptr_t g_active_material = 0;
thread_local bool g_internal_replay = false;
thread_local bool g_internal_resource_create = false;
thread_local bool g_internal_descriptor_push = false;

using parser_fn = void (__fastcall *)(void *, const void *, std::uint32_t, const wchar_t *);
using selector_fn = std::uintptr_t (__fastcall *)(
    void *, void *, std::int32_t, std::uint32_t,
    std::uint64_t, std::uint64_t, std::uint64_t,
    std::uint64_t, std::uint64_t, std::uint64_t);

parser_fn g_parser_original = nullptr;
selector_fn g_selector_original = nullptr;

std::atomic<std::uint64_t> g_present_count{0};
std::atomic<std::uint64_t> g_exe_guard_pass{0};
std::atomic<std::uint64_t> g_exe_guard_fail{0};
std::atomic<std::uint64_t> g_hook_install_pass{0};
std::atomic<std::uint64_t> g_hook_install_fail{0};
std::atomic<std::uint64_t> g_parser_seen{0};
std::atomic<std::uint64_t> g_material_mapped{0};
std::atomic<std::uint64_t> g_material_unmapped{0};
std::atomic<std::uint64_t> g_selector_seen{0};
std::atomic<std::uint64_t> g_selector_resolved{0};
std::atomic<std::uint64_t> g_native_candidates{0};
std::atomic<std::uint64_t> g_native_matches{0};
std::atomic<std::uint64_t> g_native_hash_miss{0};
std::atomic<std::uint64_t> g_virtual_created{0};
std::atomic<std::uint64_t> g_virtual_create_fail{0};
std::atomic<std::uint64_t> g_receiver_match{0};
std::atomic<std::uint64_t> g_receiver_reject{0};
std::atomic<std::uint64_t> g_material_present{0};
std::atomic<std::uint64_t> g_material_explicit_none{0};
std::atomic<std::uint64_t> g_material_unknown{0};
std::atomic<std::uint64_t> g_resource_route_hit{0};
std::atomic<std::uint64_t> g_resource_route_miss{0};
std::atomic<std::uint64_t> g_t12_rebind{0};
std::atomic<std::uint64_t> g_t14_rebind{0};
std::atomic<std::uint64_t> g_sampler_rebind{0};
std::atomic<std::uint64_t> g_replay_pass{0};
std::atomic<std::uint64_t> g_restore_pass{0};
std::atomic<std::uint64_t> g_restore_fail{0};
std::atomic<std::uint64_t> g_fail_open{0};

std::uint64_t command_id(reshade::api::command_list *cmd_list) noexcept
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(cmd_list));
}

bool sha256_bytes(const void *data, std::size_t size, std::array<std::uint8_t, 32> &digest)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD hash_size = 0;
    DWORD cb = 0;
    std::vector<std::uint8_t> object;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0) return false;
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &cb, 0);
    if (status < 0) goto cleanup;
    status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &cb, 0);
    if (status < 0 || hash_size != digest.size()) goto cleanup;
    object.resize(object_size);
    status = BCryptCreateHash(algorithm, &hash, object.data(), static_cast<ULONG>(object.size()), nullptr, 0, 0);
    if (status < 0) goto cleanup;
    if (size != 0) {
        if (size > std::numeric_limits<ULONG>::max()) { status = -1; goto cleanup; }
        status = BCryptHashData(hash, static_cast<PUCHAR>(const_cast<void *>(data)), static_cast<ULONG>(size), 0);
        if (status < 0) goto cleanup;
    }
    status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
cleanup:
    if (hash != nullptr) BCryptDestroyHash(hash);
    if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
    return status >= 0;
}

std::string hex_string(const std::uint8_t *bytes, std::size_t size)
{
    static constexpr char lut[] = "0123456789abcdef";
    std::string out(size * 2, '\0');
    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2] = lut[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = lut[bytes[i] & 0xF];
    }
    return out;
}

bool sha256_file(const std::filesystem::path &path, std::array<std::uint8_t, 32> &digest)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize size = file.tellg();
    if (size < 0) return false;
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty() && !file.read(reinterpret_cast<char *>(bytes.data()), size)) return false;
    return sha256_bytes(bytes.data(), bytes.size(), digest);
}

std::filesystem::path module_path(HMODULE module)
{
    std::array<wchar_t, 32768> path{};
    const DWORD n = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (n == 0 || n >= path.size()) return {};
    return std::filesystem::path(path.data());
}

std::filesystem::path pack_path(HMODULE addon_module)
{
    std::filesystem::path p = module_path(addon_module);
    if (p.empty()) p = std::filesystem::current_path();
    else p = p.parent_path();
    return p / L"DSRRL" / L"EnvSpec" / L"PackedGI" / L"PTDE_GI_ENVSPEC_PACK_RGBA.bin";
}

bool load_pack(HMODULE addon_module)
{
    const auto path = pack_path(addon_module);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize size = file.tellg();
    const std::size_t expected = static_cast<std::size_t>(k_probe_count) * k_ptde_slots * k_ptde_bytes_per_cube;
    if (size != static_cast<std::streamsize>(expected)) return false;
    file.seekg(0, std::ios::beg);
    g_ptde_pack.resize(expected);
    if (!file.read(reinterpret_cast<char *>(g_ptde_pack.data()), static_cast<std::streamsize>(expected))) {
        g_ptde_pack.clear();
        return false;
    }
    std::array<std::uint8_t,32> digest{};
    if (!sha256_bytes(g_ptde_pack.data(), g_ptde_pack.size(), digest) ||
        hex_string(digest.data(), digest.size()) != k_expected_pack_sha256) {
        g_ptde_pack.clear();
        return false;
    }
    return true;
}

bool read_verified_file(const std::filesystem::path &path, const char *expected_sha, std::vector<std::uint8_t> &out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize size = file.tellg();
    if (size <= 0) return false;
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char *>(out.data()), size)) { out.clear(); return false; }
