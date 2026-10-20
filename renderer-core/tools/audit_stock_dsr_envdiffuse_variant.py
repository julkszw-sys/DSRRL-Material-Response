#!/usr/bin/env python3
"""Source-contract audit for the stock-DSR-EnvDiffuse P_Metal variant."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "include/dsrrl/runtime/pmetal_stock_dsr_envdiffuse_variant.hpp").read_text("utf-8")
RUNTIME = (ROOT / "src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
MATERIALIZER = (ROOT / "src/operators/env_spec/pmetal_rgba_materializer.cpp").read_text("utf-8")
INTEGRATED = (ROOT / "integrated/integrated_addon.cpp").read_text("utf-8")

def require(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)

require("#define DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE 1" in HEADER,
        "variant policy must be source-owned")
require(RUNTIME.startswith('#include "dsrrl/runtime/pmetal_stock_dsr_envdiffuse_variant.hpp"'),
        "runtime TU must include variant policy before its gates")
require(MATERIALIZER.startswith('#include "dsrrl/runtime/pmetal_stock_dsr_envdiffuse_variant.hpp"'),
        "materializer TU must include variant policy before its gates")
require(INTEGRATED.startswith('#include "dsrrl/runtime/pmetal_stock_dsr_envdiffuse_variant.hpp"'),
        "integrated addon TU must include variant policy before logging gates")

require("#elif !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\nconstexpr char k_ptde_envdiffuse_donor_relative_path[]" in RUNTIME,
        "EnvDiffuse donor loader must be compiled out in stock mode")
install = RUNTIME.index("if (!load_exact_envdiffuse_donor())")
install_prefix = RUNTIME[max(0, install - 180):install]
require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in install_prefix,
        "install must not require the EnvDiffuse donor in stock mode")

require(RUNTIME.count("#elif defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\n    // Stock-EnvDiffuse mode is independent of the host EnvDiffuse payload.") == 2,
        "single and blend packers must not validate/capture EnvDiffuse for EnvSpec publication")
require("#if defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\n    next.envdiffuse_linear_valid = false;" in RUNTIME,
        "stock mode must not advertise a valid PTDE EnvDiffuse carrier")

marker = "#elif defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)\\n    // Explicit stock-host mode:"
require(marker in MATERIALIZER, "materializer stock mode branch must exist")
start = MATERIALIZER.index(marker)
end = MATERIALIZER.index("#else\\n    // Legacy diagnostic-only translation", start)
stock_body = MATERIALIZER[start:end]
require("apply_exact_ptde_envdiffuse_consumer" not in stock_body,
        "stock mode must not apply exact PTDE EnvDiffuse")
require("apply_linear_envdiffuse_consumer_diag" not in stock_body,
        "stock mode must not apply linear EnvDiffuse translation")
require("outcome.envdiffuse_linear_consumer_diag = false;" in stock_body,
        "stock mode must report EnvDiffuse consumer inactive")

require("#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE)" in INTEGRATED,
        "integrated logs must not claim full PTDE EnvDiffuse activation")

print("PASS: stock DSR EnvDiffuse source contracts")
print("NOTE: this audit does not build the addon or prove runtime/pixel behavior")
