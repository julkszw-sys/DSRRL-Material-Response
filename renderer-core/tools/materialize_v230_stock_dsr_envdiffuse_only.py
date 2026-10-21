#!/usr/bin/env python3
"""Single-addon RC3: original DSR EnvDiffuse t11/t13 operator, PTDE EnvSpec stays.

Run after PR484 RGBA bytecode source contract; never globally disable HemEnv
or Material Response. Exact stock source is audited at create time without
changing the released DXBC. Compile with DSRRL_V230_STOCK_DSR_ENVDIFFUSE.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STABLE = ROOT / "src/operators/env_spec/pmetal_rgba_materializer.cpp"
LERP = ROOT / "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"
RUNTIME = ROOT / "src/runtime/pmetal_envspec_draw_runtime.cpp"
FLAG = "DSRRL_V230_STOCK_DSR_ENVDIFFUSE"

def once(s: str, old: str, new: str, tag: str) -> str:
    actual = s.count(old)
    if actual != 1:
        raise RuntimeError(f"STOCK ENVDIFFUSE: drift {tag} hits={actual}")
    return s.replace(old, new, 1)

s = STABLE.read_text("utf-8")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    // Complete stable-HemEnv PTDE EnvDiffuse island:""",
    """#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    // Verify the *unchanged* retail DSR producer with the same exact
    // 29-DWORD/source+sample+multiply and DXBC checksum/precondition
    // that the PTDE materializer would normally replace. Never publish
    // the scratch result; preserve original DSR t11/s11+cb0[3],cb0[79].
    auto native_envdiffuse_source_audit = base;
    if (!apply_exact_ptde_envdiffuse_consumer(
            native_envdiffuse_source_audit)) {
        outcome.result =
            pmetal_rgba_materialize_result::fail_postcondition;
        return outcome;
    }
#elif defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    // Complete stable-HemEnv PTDE EnvDiffuse island:""",
    "stable source verifier")
s = once(s,
    "    outcome.envdiffuse_linear_consumer_diag = true;",
    """#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    outcome.envdiffuse_linear_consumer_diag = false;
#else
    outcome.envdiffuse_linear_consumer_diag = true;
#endif""",
    "stable status")
STABLE.write_text(s, "utf-8")

s = LERP.read_text("utf-8")
s = once(s,
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    if (envdiffuse_start < k_ptde_envdiffuse_lerp_prefix_words)""",
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    if (envdiffuse_start < k_ptde_envdiffuse_lerp_prefix_words)""",
    "lerp final postcondition preserve original t11 boundary")
s = once(s,
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    // R19 owns the Lerp EnvDiffuse semantic cut.""",
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    // R19 owns the Lerp EnvDiffuse semantic cut.""",
    "lerp stock bypass")
s = once(s,
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    outcome.envdiffuse_preserved = false;""",
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    outcome.envdiffuse_preserved = false;""",
    "lerp materializer readiness")
s = once(s,
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    constexpr std::size_t v344_expected_t11_delta =""",
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    constexpr std::size_t v344_expected_t11_delta =""",
    "RX33/34/35 spec-linear exact stock original t11 offset")
# Maintain R19 ABI b12[4,5] even if unused, to preserve all other shader
# resources/registers and V350 RGB swizzle. The original DSR t11/t13 block
# and common merge are checked by final_postcondition's exact saved window.
LERP.write_text(s, "utf-8")

s = RUNTIME.read_text("utf-8")
s = once(s,
    """constexpr bool k_v13_preserve_stock_envdiffuse =
    k_v13_native_dsr_no_tail_diag ||
    k_v13_native_dsr_material_mod_diag;""",
    """constexpr bool k_v13_preserve_stock_envdiffuse =
    k_v13_native_dsr_no_tail_diag ||
    k_v13_native_dsr_material_mod_diag ||
#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    true;
#else
    false;
#endif""",
    "source PTDE EnvDiffuse not required in stock mode")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        !outcome.phn_scene_encoding_composed ||
#endif
        !outcome.spec_rgb_consumer ||
        !outcome.envdiffuse_linear_consumer_diag ||""",
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        !outcome.phn_scene_encoding_composed ||
#endif
        !outcome.spec_rgb_consumer ||
#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
        outcome.envdiffuse_linear_consumer_diag ||
#else
        !outcome.envdiffuse_linear_consumer_diag ||
#endif""",
    "accept Stable source-preserved replacement while keeping SpecRGB")
s = once(s,
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        outcome.envdiffuse_preserved ||
#else
        !outcome.envdiffuse_preserved ||
#endif""",
    """#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
        outcome.envdiffuse_preserved ||
#else
        !outcome.envdiffuse_preserved ||
#endif""",
    "accept Lerp stock t11/t13 exact window")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    if (stable_envdiffuse_consumer_diag)
        carrier3 = source.envdiffuse_a;
#else
    if (stable_envdiffuse_consumer_diag)
        carrier3 = {{0.0f,0.0f,0.0f}};
#endif""",
    """#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    // Stock DSR EnvDiffuse consumes original cb0 and original t11/t13.
    // b12[3] remains EnvSpec B/beta, never a substituted PTDE diffuse gain.
#elif defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    if (stable_envdiffuse_consumer_diag)
        carrier3 = source.envdiffuse_a;
#else
    if (stable_envdiffuse_consumer_diag)
        carrier3 = {{0.0f,0.0f,0.0f}};
#endif""",
    "do not zero/add EnvDiffuse via b12 carrier")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    // Probe identity is inherited from the already-authenticated EnvSpec""",
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    // Probe identity is inherited from the already-authenticated EnvSpec""",
    "bypass PTDE t11/t13 resource pack and use captured stock SRVs")
s = once(s,
    """    prepared.native_dsr_envdiffuse_a =
        native_envdiffuse_a;
    prepared.native_dsr_envdiffuse_b =
        native_envdiffuse_b;
#endif""",
    """    prepared.native_dsr_envdiffuse_a =
        native_envdiffuse_a;
    prepared.native_dsr_envdiffuse_b =
        native_envdiffuse_b;
#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    static std::atomic<bool> stock_envdiffuse_logged{false};
    if (!stock_envdiffuse_logged.exchange(true, std::memory_order_relaxed))
        reshade::log::message(reshade::log::level::warning,
            "[DSRRL V230 STOCK DSR ENVDIFFUSE] original_DSR_t11_t13=1"
            " native_s11_s13_untouched=1 PTDE_envspec_retained=1"
            " runtime_candidate=1 draw_commit=OPEN pixel=OPEN");
#endif
#endif""",
    "explicit native resource witness")
s = once(s,
    """    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;""",
    """#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    // No claim to the independent stock EnvDiffuse source/operator.
    prepared.request.additional_owners &= ~envdiff_owner;
#endif
    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;""",
    "scope ownership after rest of PTDE island")
s = once(s,
    """#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    prepared.request.additional_constant_buffer_owners =
        mr_owner;
    prepared.request.additional_resource_owners = 0u;
#else
    prepared.request.additional_constant_buffer_owners =
        mr_owner |
        envdiff_owner;
    prepared.request.additional_resource_owners =
        envdiff_owner;
#endif""",
    """#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG) || defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    prepared.request.additional_constant_buffer_owners = mr_owner;
    prepared.request.additional_resource_owners = 0u;
#else
    prepared.request.additional_constant_buffer_owners =
        mr_owner |
        envdiff_owner;
    prepared.request.additional_resource_owners =
        envdiff_owner;
#endif""",
    "no ownership of DSR EnvDiffuse resource/CB")
s = once(s,
    """#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
        env_owner | mr_owner
#else
        env_owner | mr_owner | envdiff_owner
#endif""",
    """#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG) || defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
        env_owner | mr_owner
#else
        env_owner | mr_owner | envdiff_owner
#endif""",
    "remove EnvDiffuse owner from b12")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    prepared.request.srvs[""",
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    prepared.request.srvs[""",
    "retail t11/t13, PTDE t12/t14 exact resource binding")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    prepared.request.samplers[""",
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
    prepared.request.samplers[""",
    "retain stock DSR s11/s13 on D3D context; PTDE s12/s14 only")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
            "[DSRRL PMETAL FULL PTDE HEMENV] mode=ptde_t11_rgba_div_c86""",
    """#if defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
            "[DSRRL PMETAL STOCK DSR ENVDIFFUSE] mode=native_dsr_t11 family=%u rx=%u route=%u captured_pA=%.9g,%.9g,%.9g beta=%.9g t11=%016llx",
#elif defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
            "[DSRRL PMETAL FULL PTDE HEMENV] mode=ptde_t11_rgba_div_c86""",
    "accurate source logged for Stable")
s = once(s,
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
                    prepared.envdiffuse_resources.ptde_a
#else
                    prepared.native_dsr_envdiffuse_a""",
    """#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_V230_STOCK_DSR_ENVDIFFUSE)
                    prepared.envdiffuse_resources.ptde_a
#else
                    prepared.native_dsr_envdiffuse_a""",
    "correct log native t11 pointer")
RUNTIME.write_text(s, "utf-8")
print("V230_STOCK_DSR_ENVDIFFUSE_SOURCE_PATCH_PASS")
print("Stable stock DSR t11/s11 cb0 producer audited; Lerp stock A/B kept byte-exact;")
print("PTDE EnvSpec t12/t14, SpecRGB, MR R15/R16/R17 and GI routing untouched.")
