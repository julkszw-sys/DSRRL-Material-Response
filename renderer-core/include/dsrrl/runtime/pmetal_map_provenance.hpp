#pragma once
#include <array>
#include <cstdint>
namespace dsrrl::runtime::pmetal_map_provenance {
struct range { unsigned start, end; unsigned map; };
inline constexpr std::array<range,9> spans = {{{0,87,10},{87,130,11},{130,173,12},{173,188,13},{188,201,14},{201,274,15},{274,308,16},{308,333,17},{333,342,18}}};
constexpr unsigned probe_area(unsigned id) noexcept {for (auto r:spans) if(id>=r.start&&id<r.end)return r.map;return 0;}
}
