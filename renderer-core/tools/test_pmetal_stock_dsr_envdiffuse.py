from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT_FAIL: {message}")


cmake = read("integrated/CMakeLists.txt")
stable = read("src/operators/env_spec/pmetal_rgba_materializer.cpp")
lerp = read("src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
runtime = read("src/runtime/pmetal_envspec_draw_runtime.cpp")
pmetal_source = read("src/runtime/pmetal_env_source_runtime.cpp")
v351_materializer = read("tools/materialize_v351_typed_lightbank_modified_renderer.py")
header = read("include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp")
builder = read("tools/build_v359_stock_dsr_envdiffuse.ps1")
base_builder = read("tools/build_v350_gpu_resident_pointlight_off_rc2.ps1")
workflow = read("../.github/workflows/v359-stock-dsr-envdiffuse.yml")
contract = read("docs/PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT.md")

# The mode is opt-in and can only be built on top of the cumulative P_Metal
# EnvSpec + R19 MaterialWorkflow lineage, never as a global EnvDiffuse switch.
require("option(\n    DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE" in cmake,
        "missing opt-in CMake option")
require("NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG" in cmake and
        "NOT DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW" in cmake and
        "NOT DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE" in cmake,
        "missing cumulative-lineage dependency guard")
require("DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE" in cmake,
        "missing compile definition")

# Stable HemEnv: preserve the existing PTDE EnvSpec/SpecRGB shader island,
# skip only the EnvDiffuse rewrite, and keep independent downstream transforms.
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in stable,
        "stable PTDE EnvDiffuse consumer is not excluded in stock mode")
require("#elif !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in stable,
        "legacy EnvDiffuse consumer can still run in stock mode")
require("outcome.envdiffuse_linear_consumer_diag = false;" in stable,
        "stable stock-mode outcome does not preserve EnvDiffuse")
require("if (!apply_build131(" in stable and
        "compose_exact_phn_scene_encoding(base)" in stable and
        "compose_exact_terminal_rgb_sat(base)" in stable,
        "EnvSpec or terminal output islands were accidentally removed")

# HemEnvLerp: keep the original t11/t13 EnvDiffuse block byte-identical,
# do not extend b12 for EnvDiffuse endpoints, and keep MaterialWorkflow,
# SpecRGB and terminal-SAT materialization.
require(lerp.count("DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)") >= 6,
        "R19 b12/topology guards are incomplete")
require("#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n    // R19 owns the Lerp EnvDiffuse semantic cut." in lerp,
        "Lerp EnvDiffuse rewrite is not gated")
require("apply_exact_ptde_lerp_materialworkflow_r15_r16_r17(" in lerp,
        "R19 MaterialWorkflow path is missing")
require("apply_exact_terminal_rgb_sat(" in lerp and
        "outcome.terminal_sat_rgb_composed = true;" in lerp,
        "Lerp terminal-SAT path is missing")
require("outcome.envdiffuse_preserved = true;" in lerp,
        "Lerp does not report preserved stock EnvDiffuse")

# Runtime: no EnvDiffuse source requirement, sidecar prepare, t11/t13 SRV
# rebind, sampler write, or EnvDiffuse transaction ownership in stock mode.
require("constexpr bool k_preserve_stock_envdiffuse =" in runtime and
        "if (!k_preserve_stock_envdiffuse &&" in runtime,
        "runtime still requires PTDE EnvDiffuse source endpoints")
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n    // Probe identity" in runtime,
        "PTDE EnvDiffuse resource preparation is not excluded")
require("#elif !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE) && !defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)" in runtime,
        "native EnvDiffuse capture/rebind is not excluded")
require("#elif defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n    prepared.request.additional_owners =" in runtime,
        "stock-mode transaction owner mask is missing")
require("prepared.request.additional_resource_owners = 0u;" in runtime,
        "stock mode may claim EnvDiffuse resource ownership")
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n    prepared.request.srvs[" in runtime,
        "PTDE EnvDiffuse SRV sidecar may still be bound")
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n    prepared.request.samplers[" in runtime,
        "PTDE EnvDiffuse samplers may still be bound")
require("envspec=t12_t14=ptde" in runtime and
        "[DSRRL PMETAL STOCK DSR ENVDIFFUSE]" in runtime,
        "stock-mode runtime identity marker is missing")
require("#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) || defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in pmetal_source,
        "stock mode still compiles the EnvDiffuse donor loader as its active source path")
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\n    if (!load_exact_envdiffuse_donor())" in pmetal_source,
        "stock mode still attempts to load the PTDE EnvDiffuse donor at install")
require("#if defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\n    // Stock EnvDiffuse is supplied by the untouched DSR t11/t13 path." in pmetal_source,
        "stock mode still requires EnvDiffuse donor during source-hook publication")
require("#if defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\n            // EnvSpec still needs the exact PTDE endpoints and beta" in v351_materializer,
        "V351 typed-source gate still requires EnvDiffuse endpoints")
require("next.envdiffuse_linear_valid = false;" in pmetal_source,
        "stock source carrier does not mark EnvDiffuse endpoints unused")
require("#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n    std::array<std::uint32_t,24> payload_bits{};" in runtime,
        "runtime b12 payload width is not stock-mode aware")
require("#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\n        std::array<std::uint32_t,24> payload_bits{};" in header,
        "header b12 cache width is not stock-mode aware")

# The candidate is reproducible and remains a single integrated addon.
require("DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE=ON" in builder,
        "builder does not enable stock DSR EnvDiffuse")
require("DSRRL_V359_STOCK_DSR_ENVDIFFUSE.addon64" in builder and
        "DSRRL PMETAL STOCK DSR ENVDIFFUSE" in builder,
        "builder output identity/marker is missing")
require("$files.Count -ne 1" in base_builder and
        "Expected one addon, got" in base_builder,
        "inherited base builder does not enforce a single addon")
require("out-v359-stock-dsr-envdiffuse" in builder,
        "stock builder does not select its isolated output directory")
require("build_v359_stock_dsr_envdiffuse.ps1" in workflow and
        "actions/upload-artifact@v4" in workflow,
        "workflow does not build and publish the integrated addon")
require("shared R5 normal basis" in contract and
        "pixel equivalence remains OPEN" in contract,
        "contract must state the shared-input and pixel-validation limits")

print("PMETAL_STOCK_DSR_ENVDIFFUSE_CONTRACT_PASS")
