#pragma once
// V214: exact original DSR source-resource -> D3D11 SRV name carrier.
//
// A resource name can be inherited by a view ONLY when the named native
// resource and the view's exact backing ID3D11Resource agree. Never infer a
// resource name from FLVER alone, material aliases, SHA4, a nearby CPU name,
// a different resource, or a previous native pointer lifetime.
//
// This is identity transport, NOT permission to use PTDE SpecRGB on the
// receiver; MTD/FLVER/slot/sidecar/consumer/island checks stay independent.
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime::v214_resource_provenance {

enum class status : std::uint8_t {
    absent, exact, ambiguous
};

struct name_record {
    std::uintptr_t device = 0u;
    std::array<wchar_t, 257> chars{};
    std::uint16_t length = 0u;
    bool ambiguous = false;
};

class registry final {
    std::mutex lock_;
    std::unordered_map<std::uintptr_t, name_record> names_;
    static constexpr std::size_t k_max_records = 8192u;
public:
    // Called with a name attested by the original retail TLS producer and
    // an exact native resource handle from ReShade init_resource.
    // A repeated different name poisons this resource until destroy_resource.
    bool observe(std::uintptr_t device, std::uintptr_t resource,
                 const wchar_t *name, std::size_t n) noexcept {
        if (!device || !resource || !name || n < 3u || n > 256u)
            return false;
        for (std::size_t i = 0u; i < n; ++i)
            if (name[i] < 0x21 || name[i] > 0x7e)
                return false;

        std::lock_guard<std::mutex> guard(lock_);
        const auto it = names_.find(resource);
        if (it != names_.end()) {
            auto &record = it->second;
            if (record.ambiguous)
                return false;
            if (record.device != device || record.length != n) {
                record.ambiguous = true;
                return false;
            }
            for (std::size_t i = 0u; i < n; ++i)
                if (record.chars[i] != name[i]) {
                    record.ambiguous = true;
                    return false;
                }
            return true;
        }
        if (names_.size() >= k_max_records)
            return false;

        name_record record{};
        record.device = device;
        record.length = static_cast<std::uint16_t>(n);
        for (std::size_t i = 0u; i < n; ++i)
            record.chars[i] = name[i];
        record.chars[n] = L'\0';
        try {
            names_.emplace(resource, record);
        } catch (...) {
            return false;
        }
        return true;
    }

    // Never return a borrowed pointer into mutable registry storage.
    status snapshot(std::uintptr_t device, std::uintptr_t resource,
                    std::array<wchar_t, 257> &name,
                    std::size_t &n) noexcept {
        n = 0u;
        name.fill(L'\0');
        if (!device || !resource)
            return status::absent;
        std::lock_guard<std::mutex> guard(lock_);
        const auto it = names_.find(resource);
        if (it == names_.end())
            return status::absent;
        const auto &record = it->second;
        if (record.ambiguous || record.device != device)
            return status::ambiguous;
        n = record.length;
        name = record.chars;
        return status::exact;
    }

    void destroy(std::uintptr_t device, std::uintptr_t resource) noexcept {
        if (!device || !resource)
            return;
        std::lock_guard<std::mutex> guard(lock_);
        const auto it = names_.find(resource);
        if (it != names_.end() && it->second.device == device)
            names_.erase(it);
    }

    void destroy_device(std::uintptr_t device) noexcept {
        if (!device)
            return;
        std::lock_guard<std::mutex> guard(lock_);
        for (auto it = names_.begin(); it != names_.end();) {
            if (it->second.device == device)
                it = names_.erase(it);
            else
                ++it;
        }
    }
};

inline registry &instance() noexcept {
    static registry r;
    return r;
}
} // namespace dsrrl::runtime::v214_resource_provenance
