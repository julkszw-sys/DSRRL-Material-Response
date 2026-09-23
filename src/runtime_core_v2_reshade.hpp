#pragma once

#include "runtime_core_v2.hpp"

#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime_v2 {

// The classifier receives the full pixel-shader bytecode as well as a fast
// 64-bit fingerprint. It must return true only for an exact certified receiver.
using receiver_classifier_fn = bool (*)(
    const void *code,
    std::size_t code_size,
    std::uint64_t fast_hash,
    std::uint32_t *receiver_id,
    std::uint64_t *consumer_family_hash);

using snapshot_sink_fn = void (*)(const runtime_snapshot &snapshot);

tracker &global_tracker() noexcept;

void set_receiver_classifier(receiver_classifier_fn fn) noexcept;
void set_snapshot_sink(snapshot_sink_fn fn) noexcept;

// Call these from the existing add-on DllMain after register_addon() succeeds
// and before unregister_addon() respectively. This module has no DllMain of its
// own, so it can be merged into the current Material Response add-on.
void register_reshade_events();
void unregister_reshade_events();

} // namespace dsrrl::runtime_v2
