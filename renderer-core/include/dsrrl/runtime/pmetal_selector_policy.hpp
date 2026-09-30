#pragma once
#include <cstdint>
#include <cmath>

namespace dsrrl::runtime::pmetal_selector_policy {
// Retail 1C6900/1C6C40 + 569B40: high selector bits choose the area,
// low byte chooses the row. Type 6 falls back to common type 6 then type 5.
template<class Lookup>
auto source(std::int16_t selector, bool character, Lookup lookup) noexcept
    -> decltype(lookup(0u, 5u))
{
    const auto area = selector < 0 ? 0xffffffffu
        : (static_cast<std::uint16_t>(selector) >> 8u) & 0x7fu;
    const auto type = character ? 6u : 5u;
    auto result = area <= 11u ? lookup(area, type) : nullptr;
    if (!result) result = lookup(11u, type);
    if (!result && character) result = lookup(11u, 5u);
    return result;
}

struct endpoints { std::int16_t a, b; float beta; bool valid; };
inline endpoints select(std::int16_t a, std::int16_t b, float beta) noexcept
{
    if (!std::isfinite(beta)) return {a,b,0.0f,false};
    if (a == b || beta <= 0.0f) return {a,a,0.0f,a >= 0};
    if (beta >= 1.0f) return {b,b,0.0f,b >= 0};
    return {a,b,beta,a >= 0 && b >= 0};
}
} // namespace dsrrl::runtime::pmetal_selector_policy
