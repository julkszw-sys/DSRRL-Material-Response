#pragma once

#include "types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dsrrl::core {

struct shader_key {
    // Fast hash is only a prefilter. Full source SHA-256 is part of identity.
    std::uint64_t source_hash = 0;
    sha256_digest source_sha256{};
    std::uint32_t receiver_id = 0;
    operator_mask enabled_operators = 0;

    bool operator==(const shader_key &other) const noexcept
    {
        return source_hash == other.source_hash &&
               source_sha256 == other.source_sha256 &&
               receiver_id == other.receiver_id &&
               enabled_operators == other.enabled_operators;
    }
};

struct shader_key_hash {
    std::size_t operator()(const shader_key &key) const noexcept
    {
        std::uint64_t h = key.source_hash;
        for (const auto byte : key.source_sha256) {
            h ^= static_cast<std::uint64_t>(byte);
            h *= 1099511628211ull;
        }
        h ^= static_cast<std::uint64_t>(key.receiver_id) << 32;
        h ^= static_cast<std::uint64_t>(key.enabled_operators) * 0x9E3779B185EBCA87ull;
        return static_cast<std::size_t>(h ^ (h >> 32));
    }
};

struct shader_recipe {
    shader_key key{};
    std::uint64_t replacement_hash = 0;
    sha256_digest replacement_sha256{};
    std::uint32_t carrier_abi = 0;
};

class shader_registry {
public:
    bool register_recipe(const shader_recipe &recipe);
    std::optional<shader_recipe> resolve(const shader_key &key) const;
    std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<shader_key, shader_recipe, shader_key_hash> recipes_;
};

} // namespace dsrrl::core
