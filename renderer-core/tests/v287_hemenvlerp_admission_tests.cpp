#include "dsrrl/runtime/hemenvlerp_v287_admission.hpp"
#include <cstdint>
#include <iostream>
int main() {
    using namespace dsrrl::runtime::hemenvlerp_v287;
    sha256_digest digest{};
    if (!approved_final_v286.empty()) return 1;
    for (std::uint32_t id : {33u,34u,35u}) {
        if (permits(id,digest,22512u)) return 2;
    }
    if (permits(32,digest,22512u)||permits(36,digest,22512u)) return 3;
    std::cout << "V287_EXACT_ONLY_FAILOPEN_PASS\n";
    return 0;
}
