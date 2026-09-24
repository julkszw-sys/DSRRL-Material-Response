#include "dsrrl/operators/legacy_plan/build151_nospc_extension.hpp"

#include <cstddef>
#include <iostream>
#include <set>
#include <string>

using namespace dsrrl;

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
    using namespace operators::legacy_plan::build151;

    CHECK(k_missing24_nospc.size() == 24u);
    CHECK(k_plan_index_base == 144u);
    CHECK(k_expected_old_word == 0x07000038u);
    CHECK(k_replacement_word == 0x07000031u);

    std::set<std::string> input_sha;
    std::set<std::string> output_sha;
    std::set<std::string> names;

    std::size_t stable = 0;
    std::size_t lerp = 0;

    for (std::size_t i = 0; i < k_missing24_nospc.size(); ++i) {
        const auto &p = k_missing24_nospc[i];

        CHECK(p.representative != nullptr);
        CHECK(p.original_sha256 != nullptr);
        CHECK(p.replacement_sha256 != nullptr);
        CHECK(std::string(p.original_sha256).size() == 64u);
        CHECK(std::string(p.replacement_sha256).size() == 64u);
        CHECK(p.byte_offset % 4u == 0u);
        CHECK(p.byte_offset + 4u <= p.code_size);
        CHECK(
            static_cast<std::uint16_t>(
                k_plan_index_base + i) < 168u);

        CHECK(input_sha.insert(p.original_sha256).second);
        CHECK(output_sha.insert(p.replacement_sha256).second);
        CHECK(names.insert(p.representative).second);

        const std::string name=p.representative;
        if (name.find("HemEnvLerp") != std::string::npos)
            ++lerp;
        else if (name.find("HemEnv") != std::string::npos)
            ++stable;
        else
            CHECK(false);

        CHECK(candidate_code_size(p.code_size));
    }

    CHECK(stable == 12u);
    CHECK(lerp == 12u);
    CHECK(input_sha.size() == 24u);
    CHECK(output_sha.size() == 24u);

    core::feature_registry features;
    std::vector<std::uint8_t> replacement;

    // A non-candidate is rejected before DXBC parsing.
    std::vector<std::uint8_t> unrelated(64u, 0u);
    const auto out=materialize(
        features,
        unrelated.data(),
        unrelated.size(),
        replacement);

    CHECK(out.result == nospc_result::pass_through_not_candidate_size);
    CHECK(replacement.empty());

    std::cout << "build151_nospc_extension_tests: PASS\n";
    return 0;
}
