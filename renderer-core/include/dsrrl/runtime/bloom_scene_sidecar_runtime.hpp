#pragma once

#include "dsrrl/operators/postprocess/bloom_scene_bridge.hpp"

#include <reshade.hpp>

#include <cstdint>
#include <mutex>

struct ID3D11Device;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace dsrrl::runtime {

inline constexpr std::uint32_t k_ptde_q8_scene_width = 1024u;
inline constexpr std::uint32_t k_ptde_q8_scene_height = 720u;

struct bloom_scene_sidecar_telemetry {
    std::uint64_t init_calls = 0;
    std::uint64_t create_ok = 0;
    std::uint64_t create_fail = 0;
    std::uint64_t authorize_ok = 0;
    std::uint64_t authorize_fail = 0;
    std::uint64_t begin_ok = 0;
    std::uint64_t begin_fail = 0;
    std::uint64_t commit_ok = 0;
    std::uint64_t commit_fail = 0;
    std::uint64_t rtv_acquire_ok = 0;
    std::uint64_t srv_acquire_ok = 0;
    std::uint64_t acquire_fail = 0;
    std::uint64_t destroy_calls = 0;
    bool resource_ready = false;
    bool proof_authorized = false;
    bool contents_valid = false;
    std::uint64_t frame_serial = 0;
};

// Pixel-inert resource/lifetime carrier for the exact PTDE pre-Bloom scene.
//
// This class deliberately does not decide which draws belong to the PTDE
// writer history. It only owns the fixed 1024x720 Q8 target and refuses to
// expose it for draw replay/consumption until a complete Bloom scene proof is
// explicitly authorized. A frame is consumable only after begin_frame() and
// commit_frame() succeed for the same serial.
class bloom_scene_sidecar_runtime {
public:
    bloom_scene_sidecar_runtime() noexcept = default;
    ~bloom_scene_sidecar_runtime();

    bloom_scene_sidecar_runtime(
        const bloom_scene_sidecar_runtime &) = delete;
    bloom_scene_sidecar_runtime &operator=(
        const bloom_scene_sidecar_runtime &) = delete;

    void on_init_device(
        reshade::api::device *device) noexcept;
    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool authorize(
        const operators::postprocess::
            bloom_scene_bridge_carrier &carrier) noexcept;
    void revoke() noexcept;

    bool begin_frame(
        std::uint64_t frame_serial) noexcept;
    bool commit_frame(
        std::uint64_t frame_serial) noexcept;

    bool acquire_render_target(
        std::uint64_t frame_serial,
        ID3D11RenderTargetView **out) noexcept;
    bool acquire_shader_resource(
        std::uint64_t frame_serial,
        ID3D11ShaderResourceView **out) noexcept;

    bloom_scene_sidecar_telemetry
    telemetry() const noexcept;

    void reset() noexcept;

private:
    void release_locked() noexcept;

    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    ID3D11Texture2D *texture_ = nullptr;
    ID3D11RenderTargetView *rtv_ = nullptr;
    ID3D11ShaderResourceView *srv_ = nullptr;

    bloom_scene_sidecar_telemetry telemetry_{};
};

} // namespace dsrrl::runtime
