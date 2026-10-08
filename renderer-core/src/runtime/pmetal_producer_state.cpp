#include "dsrrl/runtime/pmetal_producer_state.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>

namespace dsrrl::runtime {
namespace {

struct producer_record {
    operators::material_response::material_identity material{};
    pmetal_envspec_source source{};
    std::uint64_t epoch = 0u;
    std::uint64_t generation = 0u;
    bool valid = false;
};

thread_local producer_record g_record{};

// R44 keyed-rendezvous diagnostic: preserve exact producer entries across
// unrelated selector attempts; reduce direct-mapped collision pressure.
constexpr std::size_t k_sync_slot_count = 4096u;
static_assert(
    (k_sync_slot_count & (k_sync_slot_count - 1u)) == 0u);

struct synchronized_slot {
    std::mutex mutex;
    producer_record record{};
    std::atomic<std::uint64_t> material_key{0u};
    std::atomic<std::uint64_t> payload_key{0u};
    std::atomic<std::uint64_t> epoch{0u};
    std::atomic_bool valid{false};
};

std::array<synchronized_slot,k_sync_slot_count> g_sync_slots{};

std::uint64_t fnv_byte(
    std::uint64_t hash,
    std::uint8_t value) noexcept
{
    hash ^= value;
    hash *= 0x100000001b3ULL;
    return hash;
}

template <typename T>
std::uint64_t fnv_scalar(
    std::uint64_t hash,
    const T &value) noexcept
{
    static_assert(
        std::is_trivially_copyable<T>::value,
        "synchronized producer key requires trivially copyable scalar");
    std::array<std::uint8_t,sizeof(T)> bytes{};
    std::memcpy(
        bytes.data(),
        &value,
        sizeof(value));
    for (const auto byte : bytes)
        hash = fnv_byte(hash, byte);
    return hash;
}

template <std::size_t N>
std::uint64_t fnv_bytes(
    std::uint64_t hash,
    const std::array<std::uint8_t,N> &bytes) noexcept
{
    for (const auto byte : bytes)
        hash = fnv_byte(hash, byte);
    return hash;
}

bool same_material(
    const operators::material_response::material_identity &a,
    const operators::material_response::material_identity &b) noexcept
{
    return
        a.valid == b.valid &&
        a.owner_tuple_exact == b.owner_tuple_exact &&
        a.material_slot_valid == b.material_slot_valid &&
        a.actual_material_exact == b.actual_material_exact &&
        a.flver_sha256 == b.flver_sha256 &&
        a.raw_mtd_sha256 == b.raw_mtd_sha256 &&
        a.material_slot == b.material_slot &&
        // flver_identity_hash is an optional legacy/cache token, not
        // authority. The complete FLVER SHA-256 + slot + semantic identity
        // above remain the exact cross-thread producer key.
        a.route_index == b.route_index &&
        a.semantic_name_hash == b.semantic_name_hash &&
        a.material_family_hash == b.material_family_hash;
}

bool same_source_payload(
    const pmetal_envspec_source &a,
    const pmetal_envspec_source &b) noexcept
{
    return
        a.a == b.a &&
        a.b == b.b &&
        a.beta == b.beta &&
        a.bank_signature_a == b.bank_signature_a &&
        a.bank_signature_b == b.bank_signature_b &&
        a.row_id_a == b.row_id_a &&
        a.row_id_b == b.row_id_b;
}

std::uint64_t material_key(
    const operators::material_response::material_identity &material) noexcept
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    hash = fnv_scalar(hash, material.valid);
    hash = fnv_scalar(hash, material.owner_tuple_exact);
    hash = fnv_scalar(hash, material.material_slot_valid);
    hash = fnv_scalar(hash, material.actual_material_exact);
    hash = fnv_bytes(hash, material.flver_sha256);
    hash = fnv_bytes(hash, material.raw_mtd_sha256);
    hash = fnv_scalar(hash, material.material_slot);
    // Do not hash flver_identity_hash here. It is optional and may be absent
    // on a later draw thread even when the authoritative FLVER SHA/slot tuple
    // is identical.
    hash = fnv_scalar(hash, material.route_index);
    hash = fnv_scalar(hash, material.semantic_name_hash);
    hash = fnv_scalar(hash, material.material_family_hash);
    return hash;
}

std::uint64_t source_key(
    const pmetal_envspec_source &source) noexcept
{
    std::uint64_t hash = 0x84222325cbf29ce4ULL;
    for (const auto value : source.a)
        hash = fnv_scalar(hash, value);
    for (const auto value : source.b)
        hash = fnv_scalar(hash, value);
    hash = fnv_scalar(hash, source.beta);
    hash = fnv_scalar(hash, source.bank_signature_a);
    hash = fnv_scalar(hash, source.bank_signature_b);
    hash = fnv_scalar(hash, source.row_id_a);
    hash = fnv_scalar(hash, source.row_id_b);
    return hash;
}

synchronized_slot &sync_slot_for(
    std::uint64_t key) noexcept
{
    const auto mixed =
        key ^ (key >> 17u) ^ (key >> 37u);
    return g_sync_slots[
        static_cast<std::size_t>(
            mixed & (k_sync_slot_count - 1u))];
}

} // namespace

void pmetal_producer_state_clear() noexcept
{
    // TLS is draw-local. The synchronized fallback is not cleared here:
    // exact P_Metal begin() invalidates only the matching material slot before
    // a new selector attempt, so unrelated FLVER selectors cannot destroy a
    // producer value needed by another command-list thread.
    g_record.valid = false;
}

void pmetal_producer_state_begin(
    const operators::material_response::material_identity &material,
    std::uint64_t epoch) noexcept
{
    const auto key = material_key(material);
    auto &slot = sync_slot_for(key);

    // This bucket may currently belong to a different verified material.
    // A selector ATTEMPT for that other material is not evidence that the
    // previous producer state has expired. Do not evict it until a successfully
    // decoded producer replaces the slot. R43 previously invalidated *every*
    // hash-bucket occupant on begin(), including unrelated P_Metal armor.
    if (slot.material_key.load(
            std::memory_order_acquire) != key)
        return;

    std::lock_guard<std::mutex> lock(slot.mutex);
    // Hashes are lookup indices, never material authority. Validate the
    // complete FLVER+MTD+slot+semantic tuple under the producer mutex.
    if (slot.material_key.load(
            std::memory_order_relaxed) != key ||
        !slot.record.valid ||
        !same_material(slot.record.material, material))
        return;

    // An attempted new selection for THIS exact material invalidates any
    // older payload. If donor resolution fails, the consumer fails open.
    slot.valid.store(
        false,
        std::memory_order_release);
    slot.epoch.store(
        epoch,
        std::memory_order_relaxed);
}

void pmetal_producer_state_publish(
    const operators::material_response::material_identity &material,
    const pmetal_envspec_source &source,
    std::uint64_t epoch) noexcept
{
    // Preserve the previous TLS payload across clear(). That lets the selector
    // identify the overwhelmingly common unchanged case without a mutex.
    const bool unchanged =
        g_record.epoch == epoch &&
        same_material(g_record.material, material) &&
        same_source_payload(g_record.source, source);

    const std::uint64_t next_generation =
        unchanged
            ? g_record.generation
            : g_record.generation + 1u;

    g_record.material = material;
    g_record.source = source;
    g_record.source.serial = epoch;
    g_record.source.generation = next_generation;
    g_record.epoch = epoch;
    g_record.generation = next_generation;
    g_record.valid = true;

    const auto key = material_key(material);
    const auto payload = source_key(g_record.source);
    auto &slot = sync_slot_for(key);

    // Same exact material/source/epoch: begin() only flipped valid=false.
    // Re-arm atomically without entering the synchronized payload path.
    if (slot.material_key.load(
            std::memory_order_acquire) == key &&
        slot.epoch.load(
            std::memory_order_acquire) == epoch &&
        slot.payload_key.load(
            std::memory_order_acquire) == payload) {
        slot.valid.store(
            true,
            std::memory_order_release);
        return;
    }

    std::lock_guard<std::mutex> lock(slot.mutex);
    slot.valid.store(
        false,
        std::memory_order_relaxed);
    slot.record = g_record;
    slot.payload_key.store(
        payload,
        std::memory_order_relaxed);
    slot.epoch.store(
        epoch,
        std::memory_order_relaxed);
    slot.material_key.store(
        key,
        std::memory_order_relaxed);
    slot.valid.store(
        true,
        std::memory_order_release);
}

bool pmetal_producer_state_latest(
    const operators::material_response::material_identity &material,
    std::uint64_t epoch,
    pmetal_envspec_source &out,
    pmetal_producer_lookup_reason *diagnostic_reason) noexcept
{
    out = {};
    const auto set_reason =
        [diagnostic_reason](
            pmetal_producer_lookup_reason reason) noexcept {
            if (diagnostic_reason != nullptr)
                *diagnostic_reason = reason;
        };

    if (g_record.valid &&
        g_record.epoch == epoch &&
        same_material(g_record.material, material)) {
        out = g_record.source;
        set_reason(pmetal_producer_lookup_reason::tls_hit);
        return true;
    }

    // This is only a typed observation frontier. Existing exact authority
    // remains the matching material identity, epoch and synchronized record.
    const auto key = material_key(material);
    auto &slot = sync_slot_for(key);
    const auto observed_key =
        slot.material_key.load(std::memory_order_acquire);
    if (observed_key != key) {
        set_reason(
            observed_key == 0u
                ? pmetal_producer_lookup_reason::no_material_bucket
                : pmetal_producer_lookup_reason::wrong_material_bucket);
        return false;
    }

    if (!slot.valid.load(std::memory_order_acquire)) {
        set_reason(pmetal_producer_lookup_reason::source_revoked);
        return false;
    }

    if (slot.epoch.load(std::memory_order_relaxed) != epoch) {
        set_reason(pmetal_producer_lookup_reason::epoch_mismatch);
        return false;
    }

    std::lock_guard<std::mutex> lock(slot.mutex);

    if (!slot.valid.load(std::memory_order_relaxed)) {
        set_reason(pmetal_producer_lookup_reason::source_revoked);
        return false;
    }
    if (slot.material_key.load(std::memory_order_relaxed) != key ||
        slot.epoch.load(std::memory_order_relaxed) != epoch ||
        !slot.record.valid ||
        slot.record.epoch != epoch ||
        !same_material(slot.record.material, material)) {
        set_reason(pmetal_producer_lookup_reason::record_identity_mismatch);
        return false;
    }

    out = slot.record.source;
    set_reason(pmetal_producer_lookup_reason::synchronized_hit);
    return true;
}

bool pmetal_producer_state_valid() noexcept
{
    return g_record.valid;
}

} // namespace dsrrl::runtime
