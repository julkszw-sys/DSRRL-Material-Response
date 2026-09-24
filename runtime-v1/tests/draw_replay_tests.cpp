#include "dsrrl/runtime/draw_replay.hpp"

#include <iostream>

using namespace dsrrl::runtime::mr;

namespace {

bool check(bool condition, const char *expression, int line)
{
    if (condition)
        return true;
    std::cerr << "CHECK FAILED line " << line << ": " << expression << '\n';
    return false;
}

#define CHECK(expr) do { if (!check(static_cast<bool>(expr), #expr, __LINE__)) return 1; } while (false)

} // namespace

int main()
{
    CHECK(choose_indexed_replay(1u, 0u) ==
          indexed_replay_kind::draw_indexed);
    CHECK(choose_indexed_replay(0u, 0u) ==
          indexed_replay_kind::draw_indexed_instanced);
    CHECK(choose_indexed_replay(1u, 7u) ==
          indexed_replay_kind::draw_indexed_instanced);
    CHECK(choose_indexed_replay(2u, 0u) ==
          indexed_replay_kind::draw_indexed_instanced);

    std::cout << "draw_replay_tests: PASS\n";
    return 0;
}
