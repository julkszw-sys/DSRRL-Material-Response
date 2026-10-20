from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT_FAIL: {message}")


cmake = read("integrated/CMakeLists.txt")
stable = read("src/operators/env_spec/pmetal_rgba_materializer.cpp")
stable_header = read("include/dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp")
lerp = read("src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
lerp_header = read("include/dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp")
runtime = read("src/runtime/pmetal_envspec_draw_runtime.cpp")
runtime_header = read("include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp")
pmetal_source = read("src/runtime/pmetal_env_source_runtime.cpp")
v351_materializer = read("tools/materialize_v351_typed_lightbank_modified_renderer.py")
integrated = read("integrated/integrated_addon.cpp")
builder = read("tools/build_v359_stock_dsr_envdiffuse.ps1")
base_builder = read("tools/build_v350_gpu_resident_pointlight_off_rc2.ps1")
workflow = read("../.github/workflows/v359-stock-dsr-envdiffuse.yml")
contract = read("docs/PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT.md")

# The stock mode keeps the cumulative R19 feature lineage, but this isolated
# candidate disables experimental nonmetal alias islands so only exact P_Metal
# can reach this EnvSpec bridge. Non-P_Metal draws must fail open.
require("option(\n    DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE" in cmake,
        "missing opt-in CMake option")
require("NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG" in cmake and
        "NOT DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW" in cmake and
        "if(NOT DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)" in cmake and
        "disables only its P_Metal EnvDiffuse rewrite" in cmake,
        "missing cumulative-lineage or P_Metal-only override guard")
require("DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE" in cmake,
        "missing compile definition")

# The default PTDE payload remains part of the cumulative source lineage, but
# this candidate disables nonmetal alias routing. Only exact P_Metal may select
# the stock payload; non-P_Metal draws fail open before source setup.
require("bool preserve_stock_envdiffuse = false" in stable_header and
        "bool preserve_stock_envdiffuse) noexcept" in stable,
        "stable materializer lacks per-payload mode")
require("if (!preserve_stock_envdiffuse)" in stable and
        "outcome.envdiffuse_linear_consumer_diag =\n        !preserve_stock_envdiffuse;" in stable,
        "stable EnvDiffuse cut is not selected per payload")
require("apply_build131(" in stable and
        "apply_exact_ptde_normal_basis_r5(" in stable and
        "remove_dsr_sao_surface_multiplier_r11(" in stable,
        "stable EnvSpec/R5/R11 cumulative operators were removed")

require("bool preserve_stock_envdiffuse = false" in lerp_header and
        "bool preserve_stock_envdiffuse) noexcept" in lerp,
        "Lerp materializer lacks per-payload mode")
require("if (!preserve_stock_envdiffuse)" in lerp and
        "preserve_stock_envdiffuse ? 64u : 96u" in lerp and
        "outcome.envdiffuse_preserved = preserve_stock_envdiffuse;" in lerp,
        "Lerp EnvDiffuse consumer/RDEF does not follow selected payload")
require("apply_exact_ptde_lerp_materialworkflow_r15_r16_r17(" in lerp and
        "apply_exact_terminal_rgb_sat(" in lerp and
        "outcome.terminal_sat_rgb_composed = true;" in lerp,
        "Lerp MaterialWorkflow/SpecRGB/terminal-SAT path is missing")

# Runtime chooses the stock variant only after the exact P_Metal material and
# route decision pass. The candidate builder disables nonmetal alias routes.
require("const bool stock_envdiffuse_for_draw =" in runtime and
        "k_stock_dsr_envdiffuse &&" in runtime and
        "exact_pmetal_material(material) &&" in runtime and
        "exact_pmetal_decision(decision);" in runtime,
        "stock variant is not gated by exact P_Metal identity")
require("stock_envdiffuse_replacements_" in runtime and
        "stock_envdiffuse_lerp_replacements_" in runtime and
        "selected_lerp_replacements" in runtime and
        "selected_replacements" in runtime,
        "per-material replacement selection maps are missing")
require("if (!preserve_stock_envdiffuse)" in runtime and
        "#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)\n    if (!preserve_stock_envdiffuse) {" in runtime,
        "EnvDiffuse sidecar resources are not skipped per stock draw")
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)\n    if (!preserve_stock_envdiffuse) {" in runtime and
        "prepared.request.srvs[" in runtime and
        "prepared.request.samplers[" in runtime,
        "SRV/sampler routing is not draw-local")
require("if (preserve_stock_envdiffuse) {" in runtime and
        "prepared.request.additional_resource_owners = 0u;" in runtime and
        "preserve_stock_envdiffuse ? 0u : envdiff_owner" in runtime,
        "stock draw still claims EnvDiffuse transaction ownership")
require("if (stock_envdiffuse_for_draw &&" in runtime and
        "[DSRRL PMETAL STOCK DSR ENVDIFFUSE]" in runtime,
        "runtime marker is not tied to the selected stock draw")
require("const std::array<f4,6> payload" in runtime and
        "const std::uint32_t payload_bytes =" in runtime and
        "desc.ByteWidth = payload_bytes;" in runtime and
        "mapped.pData,\n            payload.data(),\n            payload_bytes);" in runtime,
        "b12 ABI must switch exactly between 64 and 96 bytes")
require("struct b12_context_key" in runtime_header and
        "std::uint32_t payload_bytes = 0u;" in runtime_header and
        "stock_envdiffuse_replacements_" in runtime_header and
        "stock_envdiffuse_lerp_replacements_" in runtime_header,
        "variant maps or size-keyed b12 cache are missing")

# EnvSpec source publication, cache identity, and hook installation are
# independent of the optional EnvDiffuse endpoint. In stock mode the donor is
# not loaded, EnvDiffuse validity remains false, and source publication still
# proceeds from authenticated EnvSpec values.
require("next.envdiffuse_linear_valid =\\n        envdiffuse_linear_valid;" in pmetal_source and
        "if (donor_ok)" in pmetal_source and
        "envdiffuse_ok," in pmetal_source and
        "if (donor_ok && envdiffuse_ok)" not in pmetal_source,
        "EnvSpec publication is still coupled to EnvDiffuse validity")
require("#if !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in pmetal_source and
        "a.envdiffuse_a == b.envdiffuse_a" in pmetal_source and
        pmetal_source.index("#if !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)") <
            pmetal_source.index("a.envdiffuse_a == b.envdiffuse_a"),
        "EnvDiffuse freshness is not scoped away from the stock EnvSpec source cache")
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in pmetal_source and
        "[DSRRL PMETAL STOCK DSR ENVDIFFUSE] EnvDiffuse donor/load path disabled" in pmetal_source,
        "stock mode does not disable the EnvDiffuse donor prerequisite")
require("envdiffuse_linear_source_reject" not in runtime,
        "EnvDiffuse validity still rejects an authenticated EnvSpec draw")
require("envspec_source_finite &&" in v351_materializer and
        "(preserve_stock_envdiffuse ||" in v351_materializer and
        "envdiffuse_source_finite" in v351_materializer,
        "V351 typed-source validation is not per draw")

# Create-time registration keeps distinct stable/Lerp stock payloads. The
# nonmetal R8 candidate is not enabled in this build because V289 is OFF.
require("envspec_stock_payload" in integrated and
        "envspec_stock_payload.size()," in integrated and
        "envspec_stock_payload.size(),\n                        true" in integrated,
        "stable stock variant is not separately materialized/registered")
require("envspec_lerp_stock_payload" in integrated and
        "envspec_lerp_stock_payload.size(),\n                        true" in integrated,
        "Lerp stock variant is not separately materialized/registered")
# The base builder enables the nonmetal experimental islands for its own
# candidate. This wrapper must override each gate to OFF for P_Metal-only scope.
require("DSRRL_V285_ATOMIC_NONMETAL_DIAG=OFF" in builder and
        "DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=OFF" in builder and
        "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS=OFF" in builder and
        "V285_ATOMIC_NONMETAL_DIAG=ON" in base_builder and
        "V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON" in base_builder and
        "V293_PARSER_EXACT_FLVER_ALIAS=ON" in base_builder,
        "stock candidate must override inherited nonmetal alias islands to fail open")

candidate_gate_start = runtime.index("bool exact_pmetal_envspec_candidate(")
candidate_gate_end = runtime.index("\npmetal_envspec_draw_runtime::", candidate_gate_start)
candidate_gate = runtime[candidate_gate_start:candidate_gate_end]
require("exact_pmetal_material(material)" in candidate_gate and
        "#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)" in candidate_gate and
        "return false;" in candidate_gate,
        "non-P_Metal aliases are not rejected when V285 is disabled")

# Reproducible single-addon candidate; no release identity or source tree is
# overwritten. Construction is not runtime or pixel proof.
require("DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE=ON" in builder and
        "DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON" in builder,
        "builder does not preserve R19 lineage while enabling stock mode")
require("DSRRL_V359_STOCK_DSR_ENVDIFFUSE.addon64" in builder and
        "DSRRL PMETAL STOCK DSR ENVDIFFUSE" in builder,
        "builder output identity/marker is missing")
require("$files.Count -ne 1" in base_builder and
        "Expected one addon, got" in base_builder,
        "inherited base builder does not enforce one integrated addon")
require("out-v359-stock-dsr-envdiffuse" in builder and
        "build_v359_stock_dsr_envdiffuse.ps1" in workflow and
        "actions/upload-artifact@v4" in workflow,
        "isolated single-addon build/artifact path is missing")
require("All non-P_Metal aliases fail open" in contract and
        "exact P_Metal" in contract and
        "Pixel equivalence remains OPEN" in contract,
        "contract must distinguish the two variants and validation limits")

print("PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT_PASS")
