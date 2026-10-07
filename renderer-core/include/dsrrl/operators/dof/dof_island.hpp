#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::dof {

enum class flat_mode : std::uint8_t {
    unknown = 0,
    primary,
    alternate
};

enum class carrier_mode : std::uint8_t {
    unknown = 0,
    exact_q8_history,
    native_rate_ptde_seed
};

enum class bridge_reason : std::uint8_t {
    ready = 0,
    disabled,
    wrong_receiver,
    unknown_mode,
    incomplete_graph,
    missing_ptde_dofbank_payload,
    unresolved_ptde_dofbank_route,
    unknown_carrier,
    missing_q8_scene_history,
    missing_ptde_seed_adapter,
    missing_pass_state_transaction,
    missing_retained_pipeline_set,
    missing_private_depth_sidecar,
    missing_plain_dofrate,
    missing_fixed_raster_chain,
    incomplete_tonemap_dof_continuation,
    missing_output_cut,
    temporal_state_write_forbidden,
    stock_depth_write_forbidden
};

enum class legacy_format : std::uint8_t {
    a8r8g8b8 = 0
};

struct legacy_raster_desc {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    legacy_format format = legacy_format::a8r8g8b8;
};

struct temporal_write_set {
    bool taa_history = false;
    bool velocity = false;
    bool stock_native_depth = false;
};

struct activation_context {
    bool enabled = false;
    bool exact_imageprocess_dof_flat = false;
    flat_mode mode = flat_mode::unknown;
    carrier_mode carrier = carrier_mode::unknown;
    bool graph_complete = false;
    bool ptde_dofbank_payload_ready = false;
    bool ptde_dofbank_route_verified = false;
    bool q8_scene_history_ready = false;
    bool ptde_seed_adapter_ready = false;
    bool pass_state_transaction_ready = false;
    bool retained_flat_pipeline_set_ready = false;
    bool private_depth_sidecar_ready = false;
    bool retained_plain_dofrate_ready = false;
    bool fixed_raster_chain_ready = false;
    bool tonemap_dof_continuation_verified = false;
    bool output_cut_verified = false;
    temporal_write_set writes{};
};

struct activation_decision {
    bool active = false;
    bridge_reason reason = bridge_reason::disabled;
};

inline constexpr std::array<std::uint8_t, 9> ptde_flat_passes = {
    0x00u, 0x02u, 0x0Du, 0x01u, 0x0Du, 0x0Fu, 0x03u, 0x0Eu, 0x10u
};

inline constexpr std::array<std::uint8_t, 8> dsr_flat_passes = {
    0x01u, 0x0Du, 0x01u, 0x0Du, 0x0Fu, 0x03u, 0x0Eu, 0x10u
};

inline constexpr std::array<legacy_raster_desc, 3> ptde_fixed_raster_ladder = {{
    {1024u, 720u, legacy_format::a8r8g8b8},
    {512u, 360u, legacy_format::a8r8g8b8},
    {256u, 180u, legacy_format::a8r8g8b8}
}};

enum class retained_shader_role : std::uint8_t {
    depth_copy = 0,
    depth_copy_fragment0,
    depth_copy_fragment1,
    depth_copy_msaa,
    depth_copy_single_fragment,
    dof_composite,
    blur_upsample,
    dof_composite_cb,
    dof_rate_plain,
    dof_rate_cb,
    downsample,
    gauss_x,
    gauss_x_adv,
    gauss_y,
    gauss_y_adv,
    near_rate,
    unfocus_3x3,
    unfocus_near_rate_3x3,
    count
};

struct digest32 {
    std::array<std::uint8_t, 32> bytes{};

    constexpr bool operator==(const digest32 &other) const noexcept
    {
        for (std::size_t i = 0; i < bytes.size(); ++i)
            if (bytes[i] != other.bytes[i])
                return false;
        return true;
    }
};

constexpr std::uint8_t hex_nibble(char c) noexcept
{
    return c >= '0' && c <= '9' ? static_cast<std::uint8_t>(c - '0') :
           c >= 'a' && c <= 'f' ? static_cast<std::uint8_t>(c - 'a' + 10) :
           c >= 'A' && c <= 'F' ? static_cast<std::uint8_t>(c - 'A' + 10) :
           0xffu;
}

template<std::size_t N>
constexpr digest32 digest_from_hex(const char (&hex)[N]) noexcept
{
    static_assert(N == 65u, "SHA-256 hex string must have 64 characters");
    digest32 out{};
    for (std::size_t i = 0; i < out.bytes.size(); ++i)
        out.bytes[i] = static_cast<std::uint8_t>(
            (hex_nibble(hex[i * 2u]) << 4u) |
             hex_nibble(hex[i * 2u + 1u]));
    return out;
}

struct retained_pipeline_signature {
    retained_shader_role role = retained_shader_role::depth_copy;
    const char *name = "";
    std::uint32_t runtime_shader_id = 0u;
    std::uint16_t vertex_binder_index = 0u;
    std::uint16_t pixel_binder_index = 0u;
    std::size_t vertex_size = 0u;
    digest32 vertex_sha256{};
    std::size_t pixel_size = 0u;
    digest32 pixel_sha256{};
};

inline constexpr std::uint32_t retained_pixel_runtime_id_base = 0x0DB4u;

inline constexpr std::array<retained_pipeline_signature, 18>
retained_pipeline_signatures = {{
    {retained_shader_role::depth_copy, "FRPG_Fil_DepthCopy",
     0x0DE7u, 5u, 51u, 804u,
     digest_from_hex("6045a062806520145f2f40571ba53c5b1bdd1d638a465cb7a86a42ebc841d6af"),
     756u,
     digest_from_hex("03107ce13dcebe4ce0b3e65f0ed01e52bf606bf89603d36ca0015b3e5b0bc575")},
    {retained_shader_role::depth_copy_fragment0, "FRPG_Fil_DepthCopy_Fragment0",
     0x0DE8u, 6u, 52u, 676u,
     digest_from_hex("a94a06e16795267ebaf59303bf734bfa5c89875cf94f734a1e92df693d2c4326"),
     656u,
     digest_from_hex("53b084ceae201880781acaaee9a691f4aa8992f7b70fc7c3c8537133df77db1f")},
    {retained_shader_role::depth_copy_fragment1, "FRPG_Fil_DepthCopy_Fragment1",
     0x0DE9u, 7u, 53u, 676u,
     digest_from_hex("a94a06e16795267ebaf59303bf734bfa5c89875cf94f734a1e92df693d2c4326"),
     656u,
     digest_from_hex("28835afdf5f34e9f6f9a720c36b1e084d38a87ff258e328e46d868b68be86176")},
    {retained_shader_role::depth_copy_msaa, "FRPG_Fil_DepthCopy_MSAA",
     0x0DEAu, 8u, 54u, 4564u,
     digest_from_hex("3df5e8f220ef32e446b09cd3c8baa7a6631bb55112504fba63957963a382bfd6"),
     820u,
     digest_from_hex("e050b31f211ad2382e985594e1d0bcf3b2a32fd5725960ad8bb58f9524cb1346")},
    {retained_shader_role::depth_copy_single_fragment, "FRPG_Fil_DepthCopy_SingleFragment",
     0x0DEBu, 9u, 55u, 30968u,
     digest_from_hex("32178b7bdd8bef12a8a7dbf06a103392f36208bf6c93169fe5130daf00ae4081"),
     75948u,
     digest_from_hex("1d220e229506bb2b1aaf250a58437d55460e5028fd6df8f35a0202d8244d9711")},
    {retained_shader_role::dof_composite, "FRPG_Fil_Dof",
     0x0DECu, 10u, 56u, 892u,
     digest_from_hex("24094d1b51ac13fbc5c9b44132a9c1229e4f0e5707ad290f412500d86ff461b5"),
     5952u,
     digest_from_hex("1c135a75b8e381c9248eebd252dd48271d8cff780be77fcd4f69836873c3a645")},
    {retained_shader_role::blur_upsample, "FRPG_Fil_Dof_BlurUpSample",
     0x0DEDu, 11u, 57u, 4812u,
     digest_from_hex("41aa31540661597c0a19b924ce76bb7b0c38ed3203f8051b5341bb6e66f75c17"),
     1340u,
     digest_from_hex("6341182cb70274490c6d5527ef6f169f312afe66c126883b2e03c1eb4cbd0729")},
    {retained_shader_role::dof_composite_cb, "FRPG_Fil_Dof_CB",
     0x0DEEu, 12u, 58u, 892u,
     digest_from_hex("24094d1b51ac13fbc5c9b44132a9c1229e4f0e5707ad290f412500d86ff461b5"),
     6072u,
     digest_from_hex("141a9e3194523ffc8a14c8b275e947b81d372227f8bdffcca80422bd9a5ab9d6")},
    {retained_shader_role::dof_rate_plain, "FRPG_Fil_Dof_DofRate",
     0x0DEFu, 13u, 59u, 804u,
     digest_from_hex("6045a062806520145f2f40571ba53c5b1bdd1d638a465cb7a86a42ebc841d6af"),
     4904u,
     digest_from_hex("c75c8fc3bda694536ae9c4edec025ffc1b1c04297bf54d67c51b4b9fee782897")},
    {retained_shader_role::dof_rate_cb, "FRPG_Fil_Dof_DofRate_CB",
     0x0DF0u, 14u, 60u, 804u,
     digest_from_hex("6045a062806520145f2f40571ba53c5b1bdd1d638a465cb7a86a42ebc841d6af"),
     5020u,
     digest_from_hex("d2495d63fc90e77b13233dba6ba17506a1507228864d33b2d549c50467f7dfae")},
    {retained_shader_role::downsample, "FRPG_Fil_Dof_DownSample",
     0x0DF1u, 15u, 61u, 804u,
     digest_from_hex("6045a062806520145f2f40571ba53c5b1bdd1d638a465cb7a86a42ebc841d6af"),
     672u,
     digest_from_hex("3848869c09c19f93eb0ba586be7d7eaef85ca1befaff9a6741c225286800dd79")},
    {retained_shader_role::gauss_x, "FRPG_Fil_Dof_GaussX",
     0x0DF2u, 16u, 62u, 4988u,
     digest_from_hex("bd4293f017c11404e00e9f1d9ea076bd4b17609d12e3c5aee4380d5ad5f1aecc"),
     5204u,
     digest_from_hex("04f227ac134f351fb9403028bf2256ddb6fc5caf06eefacb6435b48dd827b3b0")},
    {retained_shader_role::gauss_x_adv, "FRPG_Fil_Dof_GaussX_Adv",
     0x0DF3u, 16u, 63u, 4988u,
     digest_from_hex("bd4293f017c11404e00e9f1d9ea076bd4b17609d12e3c5aee4380d5ad5f1aecc"),
     6052u,
     digest_from_hex("fd2c542b148793429c087c2e0ea2947098768b6f0bba1b286927b4d68ef817cc")},
    {retained_shader_role::gauss_y, "FRPG_Fil_Dof_GaussY",
     0x0DF4u, 17u, 64u, 4988u,
     digest_from_hex("a7e0d9d74f8781f6926fd0205a2fc9300e5834fc8c98090b3943db37d30a9cf6"),
     5204u,
     digest_from_hex("04f227ac134f351fb9403028bf2256ddb6fc5caf06eefacb6435b48dd827b3b0")},
    {retained_shader_role::gauss_y_adv, "FRPG_Fil_Dof_GaussY_Adv",
     0x0DF5u, 17u, 65u, 4988u,
     digest_from_hex("a7e0d9d74f8781f6926fd0205a2fc9300e5834fc8c98090b3943db37d30a9cf6"),
     6052u,
     digest_from_hex("fd2c542b148793429c087c2e0ea2947098768b6f0bba1b286927b4d68ef817cc")},
    {retained_shader_role::near_rate, "FRPG_Fil_Dof_NearRate",
     0x0DF6u, 18u, 66u, 804u,
     digest_from_hex("6045a062806520145f2f40571ba53c5b1bdd1d638a465cb7a86a42ebc841d6af"),
     4708u,
     digest_from_hex("7a2ae795170976d5544bc48abf14831cef1791a81cb4313e20618de4ae3ef763")},
    {retained_shader_role::unfocus_3x3, "FRPG_Fil_Dof_Unfocus3x3",
     0x0DF9u, 21u, 69u, 4812u,
     digest_from_hex("ed813ce909cd5b1b88239a5d5eb86cb22f28ec1bfe3c3eab3c2fb012be97b359"),
     1340u,
     digest_from_hex("391ce962719d9a186eb41d24e44581931ba3009a24d9b9a633ac152e7e7f0206")},
    {retained_shader_role::unfocus_near_rate_3x3, "FRPG_Fil_Dof_UnfocusNearRate3x3",
     0x0DFAu, 22u, 70u, 4812u,
     digest_from_hex("ed813ce909cd5b1b88239a5d5eb86cb22f28ec1bfe3c3eab3c2fb012be97b359"),
     1044u,
     digest_from_hex("017dd7e1b9b92d1f5d3c3ccffe3517f7e3e583cef97540dc90fe30958829de35")}
}};

constexpr const retained_pipeline_signature *find_retained_pipeline(
    const digest32 &vertex_sha,
    std::size_t vertex_size,
    const digest32 &pixel_sha,
    std::size_t pixel_size) noexcept
{
    for (const auto &entry : retained_pipeline_signatures)
        if (entry.vertex_size == vertex_size &&
            entry.pixel_size == pixel_size &&
            entry.vertex_sha256 == vertex_sha &&
            entry.pixel_sha256 == pixel_sha)
            return &entry;
    return nullptr;
}

struct flat_pass_shader_route {
    std::uint8_t pass = 0u;
    retained_shader_role primary = retained_shader_role::depth_copy;
    retained_shader_role alternate = retained_shader_role::count;
};

inline constexpr std::array<flat_pass_shader_route, 8>
dsr_flat_retained_routes = {{
    {0x00u, retained_shader_role::depth_copy,
            retained_shader_role::depth_copy_fragment0},
    {0x01u, retained_shader_role::depth_copy_msaa,
            retained_shader_role::count},
    {0x02u, retained_shader_role::depth_copy_msaa,
            retained_shader_role::count},
    {0x03u, retained_shader_role::depth_copy_single_fragment,
            retained_shader_role::count},
    {0x0Du, retained_shader_role::dof_rate_cb,
            retained_shader_role::count},
    {0x0Eu, retained_shader_role::downsample,
            retained_shader_role::count},
    {0x0Fu, retained_shader_role::gauss_x,
            retained_shader_role::count},
    {0x10u, retained_shader_role::gauss_y_adv,
            retained_shader_role::near_rate}
}};

inline constexpr std::uint32_t dsr_dofrate_plain_runtime_shader_id = 0x0DEFu;
inline constexpr std::uint32_t dsr_dofrate_cb_runtime_shader_id = 0x0DF0u;

constexpr bool retained_runtime_id_sequence_is_contiguous() noexcept
{
    for (const auto &entry : retained_pipeline_signatures)
        if (entry.runtime_shader_id !=
            retained_pixel_runtime_id_base + entry.pixel_binder_index)
            return false;
    return true;
}

constexpr bool flat_pass_0x10_is_output_composite() noexcept
{
    return false;
}

// Active DSR DoF handoff: Flat output target +0xF8 is sampled through
// SRV alias +0x104 by ImageProcessToneMap pass 0x13. Downstream ToneMap,
// HDR, TAA and other postprocess remain stock.
struct active_output_cut {
    std::uintptr_t image_filter_ctor = 0u;
    std::uintptr_t tonemap_ctor = 0u;
    std::uintptr_t scene_pass_builder = 0u;
    std::uintptr_t tonemap_pass13_executor_rva = 0u;
    std::uint16_t image_state_target_offset = 0u;
    std::uint16_t image_state_srv_alias_offset = 0u;
    std::uint8_t tonemap_pass = 0u;
    std::uint8_t builder_primary_srv_argument = 0u;
};

inline constexpr active_output_cut dsr_active_output_cut = {
    0x140450D30ull,
    0x140461A70ull,
    0x140452ED0ull,
    0x004572A0u,
    0x00F8u,
    0x0104u,
    0x13u,
    6u
};

constexpr bool active_output_cut_is_exact() noexcept
{
    return
        dsr_active_output_cut.image_filter_ctor == 0x140450D30ull &&
        dsr_active_output_cut.tonemap_ctor == 0x140461A70ull &&
        dsr_active_output_cut.scene_pass_builder == 0x140452ED0ull &&
        dsr_active_output_cut.tonemap_pass13_executor_rva == 0x004572A0u &&
        dsr_active_output_cut.image_state_target_offset == 0x00F8u &&
        dsr_active_output_cut.image_state_srv_alias_offset == 0x0104u &&
        dsr_active_output_cut.tonemap_pass == 0x13u &&
        dsr_active_output_cut.builder_primary_srv_argument == 6u;
}

activation_decision evaluate_activation(const activation_context &context) noexcept;

constexpr bool preserves_temporal_state(const temporal_write_set &writes) noexcept
{
    return !writes.taa_history && !writes.velocity;
}

constexpr bool preserves_stock_depth(const temporal_write_set &writes) noexcept
{
    return !writes.stock_native_depth;
}

} // namespace dsrrl::operators::dof
