#pragma once
// V298 diagnostic ONLY: parser-authenticated MTD bytes transported to the
// exact same live material pointer observed by FLVER selector. No render gate.
#include "dsrrl/core/types.hpp"
#include <array>
#include <cstdint>
#include <mutex>

namespace dsrrl::runtime::v298 {
struct parser_mtd_entry {
    const void *material = nullptr;
    dsrrl::core::sha256_digest raw{};
    std::uint32_t raw_bytes = 0u;
};
class parser_mtd_cache final {
    static constexpr std::size_t k_capacity = 4096u;
    static constexpr std::size_t k_probe = 4u;
    std::array<parser_mtd_entry,k_capacity> rows_{};
    mutable std::mutex mutex_{};
    static std::size_t position(const void *p) noexcept {
        const auto v = reinterpret_cast<std::uintptr_t>(p);
        return static_cast<std::size_t>(((v >> 4u) ^ (v >> 16u)) &
                                        (k_capacity - 1u));
    }
public:
    void erase(const void *material) noexcept {
        if (material == nullptr) return;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto index=position(material);
        for(std::size_t i=0;i<k_probe;++i) {
            auto &row=rows_[(index+i)&(k_capacity-1u)];
            if(row.material==material) row={};
        }
    }
    void publish(const void *material,
                 const dsrrl::core::sha256_digest &raw,
                 std::uint32_t raw_bytes) noexcept {
        if(material==nullptr || raw_bytes==0u) return;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto index=position(material);
        std::size_t target=index;
        for(std::size_t i=0;i<k_probe;++i) {
            const auto probe=(index+i)&(k_capacity-1u);
            if(rows_[probe].material==material ||
               rows_[probe].material==nullptr) {target=probe;break;}
        }
        rows_[target]={material,raw,raw_bytes};
    }
    bool lookup(const void *material, parser_mtd_entry &out) const noexcept {
        out={};
        if(material==nullptr)return false;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto index=position(material);
        for(std::size_t i=0;i<k_probe;++i) {
            const auto &row=rows_[(index+i)&(k_capacity-1u)];
            if(row.material==material && row.raw_bytes>0u) {
                out=row;
                return true;
            }
        }
        return false;
    }
};
} // namespace dsrrl::runtime::v298
