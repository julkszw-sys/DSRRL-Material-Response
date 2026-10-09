#pragma once
// DSRRL PR292 diagnostic-only ReShade D3D11 list provenance.
// AFTER_FINISH is not EXECUTE; BEFORE_EXECUTE is not proof of GPU completion.
// CPU updates and P_Metal prepare candidates do not prove native draw contents.
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace dsrrl::runtime::pmetal_asylum_cmdlist_trace {
constexpr std::uint64_t m10_bank = 0x4c594553d201d80cULL;
constexpr std::uint64_t m18_bank = 0x1ecfd1e617c59071ULL;

enum class stage : std::uint8_t { none, after_finish, before_execute };
struct snapshot {
    stage phase = stage::none;
    std::uintptr_t recorded_api = 0, recorded_native = 0;
    std::uintptr_t finished_api = 0, execution_api = 0;
    std::uint64_t generation = 0;
    std::uint32_t size_matched_b0_updates = 0, size_matched_b1_updates = 0;
    std::uint32_t rx33_candidates = 0, rx34_candidates = 0;
    std::uint32_t owner_sha_prefix = 0, slot = 0, rx = 0, row = 0;
    std::uint32_t bank_mask = 0; // bit0=m10, bit1=m18; 3=mixed
};
class observer final {
    struct ctx_slot {
        std::uintptr_t key = 0, native = 0;
        std::uint64_t generation = 1;
        bool pending_close = false;
        snapshot observed{};
    };
    struct finished_slot {
        std::uintptr_t key = 0;
        snapshot recorded{};
    };
    std::array<ctx_slot, 128> contexts_{};
    std::array<finished_slot, 256> finished_{};
    std::mutex mutex_{};
    ctx_slot *find_ctx(std::uintptr_t key, bool insert) noexcept {
        if (!key) return nullptr;
        ctx_slot *vacant = nullptr;
        for (auto &c: contexts_) {
            if (c.key == key) return &c;
            if (!vacant && !c.key) vacant = &c;
        }
        if (!insert || !vacant) return nullptr;
        vacant->key = key;
        return vacant;
    }
    finished_slot *find_finished(std::uintptr_t key, bool insert) noexcept {
        if (!key) return nullptr;
        finished_slot *vacant = nullptr;
        for (auto &f: finished_) {
            if (f.key == key) return &f;
            if (!vacant && !f.key) vacant = &f;
        }
        if (!insert || !vacant) return nullptr;
        vacant->key = key;
        return vacant;
    }
    static void increment(std::uint32_t &value) noexcept {
        if (value != UINT32_MAX) ++value;
    }
public:
    observer() = default;
    observer(const observer &) = delete;
    observer &operator=(const observer &) = delete;

    void observe_deferred_update(std::uintptr_t key,
                                 std::uintptr_t native,
                                 std::uint64_t offset,
                                 std::uint64_t size) noexcept {
        if (offset != 0 || (size != 2064 && size != 48)) return;
        std::lock_guard<std::mutex> guard(mutex_);
        auto *ctx = find_ctx(key, true);
        if (!ctx || ctx->pending_close) return;
        ctx->native = native;
        if (size == 2064) increment(ctx->observed.size_matched_b0_updates);
        else increment(ctx->observed.size_matched_b1_updates);
    }
    void observe_receiver_candidate(std::uintptr_t key,
                                    std::uintptr_t native,
                                    std::uint32_t rx,
                                    std::uint64_t bank,
                                    std::uint32_t row,
                                    std::uint32_t sha_prefix,
                                    std::uint32_t slot) noexcept {
        if ((rx != 33 && rx != 34) ||
            (bank != m10_bank && bank != m18_bank)) return;
        std::lock_guard<std::mutex> guard(mutex_);
        auto *ctx = find_ctx(key, true);
        if (!ctx || ctx->pending_close) return;
        ctx->native = native;
        auto &v = ctx->observed;
        if (rx == 33) increment(v.rx33_candidates);
        else increment(v.rx34_candidates);
        v.bank_mask |= bank == m10_bank ? 1u : 2u;
        v.owner_sha_prefix = sha_prefix;
        v.slot = slot;
        v.rx = rx;
        v.row = row;
    }
    void close(std::uintptr_t key, std::uintptr_t native) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        auto *ctx = find_ctx(key, false);
        if (!ctx) return;
        ctx->native = native;
        ctx->pending_close = true;
    }
    snapshot secondary(std::uintptr_t first,
                       std::uintptr_t second) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        // Pinned ReShade D3D11 6.8.0.1: (context, list) BEFORE ExecuteCommandList.
        if (auto *list = find_finished(second, false)) {
            auto result = list->recorded;
            result.phase = stage::before_execute;
            result.execution_api = first;
            return result;
        }
        // Pinned ReShade: (fresh finished-list, deferred context) AFTER FinishCommandList.
        auto *ctx = find_ctx(second, false);
        if (!ctx || !ctx->pending_close || !first || first == second)
            return {};
        ctx->pending_close = false;
        const auto observed = ctx->observed;
        ctx->observed = {};
        const auto generation = ctx->generation++;
        const bool relevant = observed.rx33_candidates || observed.rx34_candidates ||
                              observed.size_matched_b0_updates || observed.size_matched_b1_updates;
        if (!relevant) return {};
        auto *list = find_finished(first, true);
        if (!list) return {}; // capacity exhausted: no guessed association
        list->recorded = observed;
        list->recorded.phase = stage::after_finish;
        list->recorded.recorded_api = second;
        list->recorded.recorded_native = ctx->native;
        list->recorded.finished_api = first;
        list->recorded.generation = generation;
        return list->recorded;
    }
    void reset_command(std::uintptr_t key) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        auto *ctx = find_ctx(key, false);
        if (!ctx) return;
        ctx->pending_close = false;
        if (ctx->observed.rx33_candidates || ctx->observed.rx34_candidates ||
            ctx->observed.size_matched_b0_updates ||
            ctx->observed.size_matched_b1_updates) {
            ctx->observed = {};
            ++ctx->generation;
        }
    }
    void destroy_command(std::uintptr_t key) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto &f: finished_) if (f.key == key) f = {};
        for (auto &c: contexts_) if (c.key == key) c = {};
    }
    void clear() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto &c: contexts_) c = {};
        for (auto &f: finished_) f = {};
    }
};

// One singleton per addon binary across translation units, not one per hook.
inline observer &global_observer() noexcept {
    static observer value;
    return value;
}
} // namespace dsrrl::runtime::pmetal_asylum_cmdlist_trace
