cmake_minimum_required(VERSION 3.20)

project(DSRRLCoreIslands LANGUAGES CXX ASM_MASM)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT WIN32)
    message(FATAL_ERROR "DSRRL Core+Islands addon is Windows-only.")
endif()

file(
    READ
    "${CMAKE_CURRENT_LIST_DIR}/VERSION.json"
    DSRRL_CORE_ISLANDS_VERSION_JSON)

string(
    JSON
    DSRRL_CORE_ISLANDS_VERSION
    GET
    "${DSRRL_CORE_ISLANDS_VERSION_JSON}"
    version)

string(
    JSON
    DSRRL_CORE_ISLANDS_PRODUCT_LINE
    GET
    "${DSRRL_CORE_ISLANDS_VERSION_JSON}"
    product_line)

string(
    JSON
    DSRRL_CORE_ISLANDS_ARTIFACT_STEM
    GET
    "${DSRRL_CORE_ISLANDS_VERSION_JSON}"
    artifact_stem)

set(
    DSRRL_SOURCE_COMMIT
    "unknown"
    CACHE STRING
    "Source commit embedded into runtime telemetry")

set(
    DSRRL_BUILD_FLAVOR
    "default"
    CACHE STRING
    "Build flavor/ref embedded into runtime telemetry")

option(
    DSRRL_DIAGNOSTIC_TELEMETRY_DEFAULT_ON
    "Compile Core+Islands with runtime/effect/native-state telemetry always enabled"
    OFF)

option(
    DSRRL_FULL_TELEMETRY_DEFAULT_ON
    "Compile Core+Islands with runtime/effect telemetry enabled without native-state verification"
    OFF)

option(
    DSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY
    "Compile Core+Islands with low-overhead effect activation telemetry enabled by default"
    OFF)

option(
    DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED
    "Diagnostic: feed exact native DSR BC6H cubemaps into the PTDE P_Metal EnvSpec operator"
    OFF)

option(
    DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG
    "Diagnostic: exact V13-style P_Metal EnvSpec over native DSR cubemaps without the modern SpecRGB/c101/COLOR0 final tail; includes stable HemEnv and exact paired HemEnvLerp"
    OFF)

option(
    DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG
    "Diagnostic: PR203 V13/native-DSR EnvSpec plus only PTDE SpecRGB*c101*COLOR0 modulation on the EnvSpec accumulator before the preserved common merge; stable HemEnv + paired HemEnvLerp"
    OFF)

option(
    DSRRL_PMETAL_V13_PTDE_PACKEDGI_MATERIAL_MOD_DIAG
    "Diagnostic: PR204 material-modulation shader/operator with exact PTDE PackedGI cubemap carrier instead of native DSR BC6H; stock EnvDiffuse/common merge; stable HemEnv + paired HemEnvLerp"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG
    "Diagnostic: stable P_Metal full PTDE HemEnv with exact PTDE EnvSpec + EnvDiffuse PackedGI carriers and exact PTDE LightBank endpoints"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R3
    "Diagnostic: PR214 R3 exact SpecRGB attestation plus removal of DSR-only post-merge environment scalar on the full PTDE stable HemEnv path"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R4
    "Diagnostic: PR215 R4 samples exact PTDE equipment SpecRGB directly as RGB instead of inheriting the stock DSR t1 wxyz channel remap"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R5
    "Diagnostic: exact PTDE stable-HemEnv active normal-basis differences for P_Metal (no SV_IsFrontFace normal flip, per-pixel bitangent cross; preserve identity LightProbeParam.z path)"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R6
    "Cumulative stable P_Metal PTDE HemEnv successor: R3+R4+R5 plus source-gated PHN k135 terminal consumer; atmosphere continuation follows the selected lineage"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW
    "Stable P_Metal Csd/Sdw: replace DSR 3x3 comparison shadow visibility producer with exact PTDE 4x4 packed-depth PCF16 and exact regular stage-7 sampler"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB
    "Stable P_Metal cumulative successor: restore recovered PTDE PRE V2.28A SPEC-domain linear carrier and V2.28B angular/horizon bypass before Build131"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE
    "Stable P_Metal cumulative successor: remove the DSR-only t8 SAO whole-surface multiplier after the PTDE-equivalent common environment/material merge"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR
    "P_Metal R15 diagnostic: zero the DSR-only MaterialWorkflow additive live-out and neutralize the downstream SpecTex-alpha common-surface gate"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT
    "P_Metal R16 diagnostic: remove DSR-only SpecTex-alpha and metalness/workflow-weight suppression from the common diffuse carrier"
    OFF)

option(
    DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL
    "P_Metal R17 diagnostic: replace the inner DSR MaterialWorkflow cb9->cb10 blend with direct PTDE c100 material selection"
    OFF)

option(
    DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW
    "P_Metal R19: exact HemEnvLerp MaterialWorkflow topology port (R15 additive/postmerge gates, R16 diffuse split, R17 direct PTDE c100 producer)"
    OFF)

option(
    DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE
    "P_Metal R19: exact PTDE HemEnvLerp EnvDiffuse A/B+beta consumer over paired PTDE RGBA probe resources; extends b12 only for the Lerp endpoint carrier"
    OFF)

option(
    DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL
    "Physically remove fixed R40 and clustered PntS/Spc PointLight runtime producer/pipeline/draw units; preserve stock DSR PointLight"
    OFF)

option(
    DSRRL_POINTLIGHT_DRAWTIME_BYPASS
    "Diagnostic: disable PointLight producer hooks, runtime pipeline binding and draw-time carrier/replay while preserving all non-PointLight runtime paths"
    OFF)

option(
    DSRRL_DRAWTIME_ISLANDS_BYPASS
    "Diagnostic: disable all draw-time island routing/hooks/replay while preserving create-time A1 and MotionBlur shader replacements"
    OFF)

option(
    DSRRL_DRAW_CALLBACKS_BYPASS
    "Diagnostic: keep dynamic transports/hooks installed but omit draw/draw_indexed/present callbacks"
    OFF)

option(
    DSRRL_DRAW_REPLAY_BYPASS
    "Diagnostic: keep dynamic draw routing and preparation but release prepared islands before replay/restore"
    OFF)

option(
    DSRRL_EMPTY_DRAW_CALLBACK_BISECT
    "Diagnostic: keep dynamic transports and register draw callbacks, but return immediately before any DSRRL draw work"
    OFF)

option(
    DSRRL_STATE_TRANSACTION_ONLY_BISECT
    "Diagnostic: execute draw transaction begin+restore without issuing a replacement Draw/DrawIndexed"
    OFF)

option(
    DSRRL_RAW_DRAW_REPLAY_BISECT
    "Diagnostic: issue only a raw stock Draw/DrawIndexed from the callback and suppress the original, without DSRRL state mutation"
    OFF)

option(
    DSRRL_STATE_CAPTURE_ONLY_BISECT
    "Diagnostic: run native PS state capture only for prepared island mutations; no Set, core transaction, replay or restore"
    OFF)

option(
    DSRRL_NATIVE_STATE_MUTATE_RESTORE_ONLY_BISECT
    "Diagnostic: run native D3D11 state capture+mutation+restore without core transaction bookkeeping or replay"
    OFF)

option(
    DSRRL_CORE_TRANSACTION_ONLY_BISECT
    "Diagnostic: run renderer-core transaction begin+restore bookkeeping only, without native PS Get/Set or replay"
    OFF)

option(
    DSRRL_RAW_NATIVE_DRAW_REENTRY_MIN_BISECT
    "Diagnostic: minimal native Draw/DrawIndexed re-entry from ReShade callback with recursion guard and no semantic preparation"
    OFF)

option(
    DSRRL_ADDON_LOADED_ONLY_BISECT
    "Diagnostic: register the addon only; install no DSRRL events, pipeline callbacks, draw callbacks or runtime transports"
    OFF)

option(
    DSRRL_DRAW_CALLBACK_ONLY_BISECT
    "Diagnostic: register only empty draw/draw_indexed callbacks; install no DSRRL pipeline events or runtime transports"
    OFF)

option(
    DSRRL_PMETAL_SRV_SHADOW_EXPERIMENT
    "Experimental Runtime v2 pixel-SRV bind shadow; disabled by default to avoid global descriptor-event overhead"
    OFF)

option(
    DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE
    "Diagnostic: physically exclude Upper/Lower, HemDir3 and Subsurface runtime implementation units while preserving P_Metal source runtime"
    ON)

option(
    DSRRL_FLVER_SELECTOR_PROFILE
    "Diagnostic: low-overhead sampled FLVER selector/support interaction profiler"
    OFF)
option(
    DSRRL_STUTTER_PROFILE
    "Record bounded QPC timing for R43 streaming stutter, no renderer feature changes"
    OFF)
option(
    DSRRL_STUTTER_HITCH_TRACE
    "Opt-in: correlate worst Present gaps to completed FLVER, DDS, MTD and registry hook spans by thread"
    OFF)

option(
    DSRRL_RESOURCE_EPOCH_SHARD_SYNC
    "Experimental: invalidate companion SRV TLS by 256 key buckets instead of process-global epoch"
    OFF)
option(DSRRL_COMPANION_TLS_4WAY
    "Experimental: 64 sets x4 ways instead of direct-mapped TLS; same 256 total entries"
    OFF)
option(DSRRL_MR_MATERIAL_TRACE
    "Diagnostic: bounded material MTD/receiver decision and draw outcome census for localized lighting mismatches"
    OFF)
option(DSRRL_PMETAL_SPEC_CUT_TRACE
    "Diagnostic: per-owner sampled P_Metal SpecRGB t1 registry and PTDE/stock transition trace"
    OFF)

option(
    DSRRL_POINTLIGHT_PROFILE
    "Diagnostic: low-overhead sampled clustered PointLight producer/transaction profiler"
    OFF)

# Release-preparation profile deliberately makes no claims about PTDE pixel
# equivalence. It only removes heavyweight one-shot operator diagnostics and
# rejects build-time bisect configurations which cannot be shipped safely.
option(
    DSRRL_RELEASE_CLEANUP
    "Strip P_Metal per-draw diagnostic probes/logging; enforce physically cut fail-open island set"
    OFF)

if(DSRRL_RELEASE_CLEANUP)
    if(NOT DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL OR
       NOT DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE)
        message(FATAL_ERROR
            "Release cleanup requires PointLight and UL/H3/Subsurface physically cut")
    endif()

    if(DSRRL_DIAGNOSTIC_TELEMETRY_DEFAULT_ON OR
       DSRRL_FULL_TELEMETRY_DEFAULT_ON OR
       DSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY OR
       DSRRL_PMETAL_SRV_SHADOW_EXPERIMENT OR
       DSRRL_FLVER_SELECTOR_PROFILE OR
       DSRRL_POINTLIGHT_PROFILE OR
       DSRRL_DRAWTIME_ISLANDS_BYPASS OR
       DSRRL_DRAW_CALLBACKS_BYPASS OR
       DSRRL_DRAW_REPLAY_BYPASS OR
       DSRRL_EMPTY_DRAW_CALLBACK_BISECT OR
       DSRRL_STATE_TRANSACTION_ONLY_BISECT OR
       DSRRL_RAW_DRAW_REPLAY_BISECT OR
       DSRRL_STATE_CAPTURE_ONLY_BISECT OR
       DSRRL_NATIVE_STATE_MUTATE_RESTORE_ONLY_BISECT OR
       DSRRL_CORE_TRANSACTION_ONLY_BISECT OR
       DSRRL_RAW_NATIVE_DRAW_REENTRY_MIN_BISECT OR
       DSRRL_ADDON_LOADED_ONLY_BISECT OR
       DSRRL_DRAW_CALLBACK_ONLY_BISECT)
        message(FATAL_ERROR
            "Release cleanup rejects all diagnostic telemetry defaults, SRV shadow experiments, profilers and draw bisects")
    endif()
endif()

set(
    RESHADE_INCLUDE_DIR
    ""
    CACHE PATH
    "Path to pinned ReShade include directory")

if(NOT RESHADE_INCLUDE_DIR)
    message(
        FATAL_ERROR
        "Set -DRESHADE_INCLUDE_DIR=PATH/TO/reshade/include")
endif()

get_filename_component(
    DSRRL_RENDERER_CORE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.."
    ABSOLUTE)

set(
    DSRRL_CORE_GENERATED_ROOT
    "${CMAKE_BINARY_DIR}/generated")
set(
    DSRRL_CORE_GENERATED_RUNTIME
    "${DSRRL_CORE_GENERATED_ROOT}/dsrrl/runtime")
file(MAKE_DIRECTORY "${DSRRL_CORE_GENERATED_RUNTIME}")

set(
    DSRRL_CORE_GENERATED_LIGHTBANK
    "${DSRRL_CORE_GENERATED_ROOT}/dsrrl/operators/lightbank")
file(MAKE_DIRECTORY "${DSRRL_CORE_GENERATED_LIGHTBANK}")

set(
    DSRRL_CORE_GENERATED_MATERIAL_RESPONSE
    "${DSRRL_CORE_GENERATED_ROOT}/dsrrl/operators/material_response")
file(MAKE_DIRECTORY "${DSRRL_CORE_GENERATED_MATERIAL_RESPONSE}")

set(
    DSRRL_CORE_GENERATED_ENVSPEC
    "${DSRRL_CORE_GENERATED_ROOT}/dsrrl/operators/env_spec")
file(MAKE_DIRECTORY "${DSRRL_CORE_GENERATED_ENVSPEC}")

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_RUNTIME}/generated_spec_routes_v12.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_spec_routes_v12.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/spec_routes_v12.tsv"
            --output "${DSRRL_CORE_GENERATED_RUNTIME}/generated_spec_routes_v12.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_spec_routes_v12.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/spec_routes_v12.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_RUNTIME}/generated_diffuse_routes_v12.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_diffuse_routes_v12.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/diffuse_routes_v12.tsv"
            --output "${DSRRL_CORE_GENERATED_RUNTIME}/generated_diffuse_routes_v12.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_diffuse_routes_v12.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/diffuse_routes_v12.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_RUNTIME}/generated_normal_routes_v12.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_normal_routes_v12.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/normal_routes_v12.tsv"
            --output "${DSRRL_CORE_GENERATED_RUNTIME}/generated_normal_routes_v12.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_normal_routes_v12.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/normal_routes_v12.tsv"
    VERBATIM)

add_custom_target(
    dsrrl_core_material_resource_routes
    DEPENDS
        "${DSRRL_CORE_GENERATED_RUNTIME}/generated_spec_routes_v12.hpp"
        "${DSRRL_CORE_GENERATED_RUNTIME}/generated_diffuse_routes_v12.hpp"
        "${DSRRL_CORE_GENERATED_RUNTIME}/generated_normal_routes_v12.hpp")

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_receivers_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_hemdir3_receiver_registry_v1.py"
            --receivers "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_receivers_v1.tsv"
            --patch-sites "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_b13_patch_sites_v1.tsv"
            --replacements "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_b13_replacements_v1.tsv"
            --out "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_receivers_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_hemdir3_receiver_registry_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_receivers_v1.tsv"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_b13_patch_sites_v1.tsv"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_b13_replacements_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_spc_b12_authority_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_hemdir3_spc_b12_authority_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_spc_b12_patch_sites_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_spc_b12_authority_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_hemdir3_spc_b12_authority_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_spc_b12_patch_sites_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_spc_material_donors_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_hemdir3_spc_material_donors_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_spc_material_donors_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_spc_material_donors_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_hemdir3_spc_material_donors_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/hemdir3_spc_material_donors_v1.tsv"
    VERBATIM)

add_custom_target(
    dsrrl_core_hemdir3_authority
    DEPENDS
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_receivers_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_spc_b12_authority_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_hemdir3_spc_material_donors_v1.hpp")

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenv_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_hemenv_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_hemenv_consumers_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenv_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_hemenv_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_hemenv_consumers_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenvlerp_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_hemenvlerp_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_hemenvlerp_consumers_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenvlerp_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_hemenvlerp_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_hemenvlerp_consumers_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_parallax_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_parallax_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_parallax_unique_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_parallax_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_parallax_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_parallax_unique_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_pnts_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_pnts_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_pnts_unique_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_pnts_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_pnts_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_pnts_unique_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_faceeye_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_faceeye_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_faceeye_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_faceeye_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_faceeye_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_faceeye_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_subsurf_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_subsurf_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_subsurf_unique_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_subsurf_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_phn_subsurf_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_phn_subsurf_unique_v1.tsv"
    VERBATIM)

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_nonphn_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_nonphn_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_gst_v1.tsv"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_gst_faceeye_v1.tsv"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_sfx_v1.tsv"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_snow_v1.tsv"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_ntoa_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_nonphn_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_upper_lower_nonphn_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_gst_v1.tsv"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_gst_faceeye_v1.tsv"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_sfx_v1.tsv"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_snow_v1.tsv"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/upper_lower_ntoa_v1.tsv"
    VERBATIM)

add_custom_target(
    dsrrl_core_upper_lower_authority
    DEPENDS
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenv_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenvlerp_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_parallax_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_pnts_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_faceeye_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_phn_subsurf_v1.hpp"
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_nonphn_v1.hpp")

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_RUNTIME}/generated_pmetal_env_source_authority.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_pmetal_env_source_authority.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/provenance/mr145/SOURCE_V13_PTDE_DONOR.cpp"
            --layout-input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/pmetal_env_source_layout_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_RUNTIME}/generated_pmetal_env_source_authority.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_pmetal_env_source_authority.py"
        "${DSRRL_RENDERER_CORE_ROOT}/provenance/mr145/SOURCE_V13_PTDE_DONOR.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/pmetal_env_source_layout_v1.tsv"
    VERBATIM)

add_custom_target(
    dsrrl_core_pmetal_env_source_authority
    DEPENDS
        "${DSRRL_CORE_GENERATED_RUNTIME}/generated_pmetal_env_source_authority.hpp")

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_MATERIAL_RESPONSE}/generated_diffuse_response_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_material_response_diffuse_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/material_response_diffuse_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_MATERIAL_RESPONSE}/generated_diffuse_response_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_material_response_diffuse_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/material_response_diffuse_v1.tsv"
    VERBATIM)

add_custom_target(
    dsrrl_core_material_response_diffuse_v1_authority
    DEPENDS
        "${DSRRL_CORE_GENERATED_MATERIAL_RESPONSE}/generated_diffuse_response_v1.hpp")

add_custom_command(
    OUTPUT "${DSRRL_CORE_GENERATED_ENVSPEC}/generated_pmetal_envspec_hemenvlerp_v1.hpp"
    COMMAND "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_pmetal_envspec_hemenvlerp_v1.py"
            --input "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/pmetal_envspec_hemenvlerp_v1.tsv"
            --output "${DSRRL_CORE_GENERATED_ENVSPEC}/generated_pmetal_envspec_hemenvlerp_v1.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_pmetal_envspec_hemenvlerp_v1.py"
        "${DSRRL_RENDERER_CORE_ROOT}/data/provenance/pmetal_envspec_hemenvlerp_v1.tsv"
    VERBATIM)

add_custom_target(
    dsrrl_core_pmetal_envspec_hemenvlerp_authority
    DEPENDS
        "${DSRRL_CORE_GENERATED_ENVSPEC}/generated_pmetal_envspec_hemenvlerp_v1.hpp")

set(
    RESHade_INCLUDE_DIR
    "${RESHADE_INCLUDE_DIR}"
    CACHE PATH
    "Path to pinned ReShade include directory"
    FORCE)

add_subdirectory(
    "${DSRRL_RENDERER_CORE_ROOT}"
    "${CMAKE_BINARY_DIR}/renderer-core-lib"
    EXCLUDE_FROM_ALL)

set(
    DSRRL_INTEGRATED_ADDON_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/integrated_addon.cpp")

if(DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE)
    set(
        DSRRL_INTEGRATED_ADDON_SOURCE
        "${DSRRL_CORE_GENERATED_RUNTIME}/integrated_addon_physical_cut.cpp")
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_runtime_v2_physical_cut.py"
            --input "${CMAKE_CURRENT_LIST_DIR}/integrated_addon.cpp"
            --output "${DSRRL_INTEGRATED_ADDON_SOURCE}"
        RESULT_VARIABLE DSRRL_PHYSICAL_CUT_GENERATE_RESULT)
    if(NOT DSRRL_PHYSICAL_CUT_GENERATE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate Runtime v2 physical-cut integrated source")
    endif()

    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/audit_runtime_v2_physical_cut.py"
            --source-dir "${DSRRL_RENDERER_CORE_ROOT}"
        RESULT_VARIABLE DSRRL_PHYSICAL_CUT_SOURCE_AUDIT_RESULT)
    if(NOT DSRRL_PHYSICAL_CUT_SOURCE_AUDIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Runtime v2 physical-cut source audit failed")
    endif()
endif()

add_library(
    dsrrl_renderer_core_integrated
    SHARED
    "${DSRRL_INTEGRATED_ADDON_SOURCE}"
    flver_identity_detour.asm
    texture_identity_detour.asm
    hemdir3_mode_detour.asm
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/a1_create_pipeline_bridge.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/flver_engine_hooks.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/texture_identity_transport.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/material_resource_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/envspec_resource_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/bloom_scene_sidecar_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/bloom_fx_draw_transport.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_envspec_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_env_source_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_producer_state.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_native_draw_bridge.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pixel_srv_shadow.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemenvlerp_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/subsurface_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_mode_transport.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/draw_state_transaction.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/island_draw_adapter.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/material_response_draw_transaction.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/material_response/material_response_diffuse_v1.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/resource_bridges/spec_rgb_consumer_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/resource_bridges/fixed_pointlight_spec_rgb_bridge.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/env_spec/pmetal_rgba_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/lightbank/hemdir3_b13_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/lightbank/upper_lower_hemenv_materializer.cpp")

# Compile exactly one PointLight ABI carrier: real runtime sources or an inert
# fail-open linker surface. The physical-cut branch never compiles real R40/
# clustered Spc PointLight producer, shaders, callbacks, resource or draw units.
if(DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL)
    target_sources(
        dsrrl_renderer_core_integrated
        PRIVATE
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pointlight_physical_cut_noop.cpp")
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL
        DSRRL_POINTLIGHT_DRAWTIME_BYPASS)
else()
    target_sources(
        dsrrl_renderer_core_integrated
        PRIVATE
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/fixed_pointlight_draw_runtime.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/clustered_pnts_draw_runtime.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/clustered_pnts_pipeline_runtime.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/fixed_pointlight_pipeline_runtime.cpp")
endif()

if(NOT DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE)
    target_sources(
        dsrrl_renderer_core_integrated
        PRIVATE
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_draw_runtime.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_draw_runtime.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/subsurface_draw_runtime.cpp"
        "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_hemenv_draw_runtime.cpp")
endif()

add_dependencies(
    dsrrl_renderer_core_integrated
    dsrrl_mr_exact_binding_header
    dsrrl_core_material_resource_routes
    dsrrl_core_hemdir3_authority
    dsrrl_core_upper_lower_authority
    dsrrl_core_pmetal_env_source_authority
    dsrrl_core_material_response_diffuse_v1_authority
    dsrrl_core_pmetal_envspec_hemenvlerp_authority)

target_include_directories(
    dsrrl_renderer_core_integrated
    PRIVATE
    "${DSRRL_RENDERER_CORE_ROOT}/include"
    "${DSRRL_CORE_GENERATED_ROOT}"
    "${RESHADE_INCLUDE_DIR}")

target_link_libraries(
    dsrrl_renderer_core_integrated
    PRIVATE
    dsrrl_renderer_core)

target_compile_features(
    dsrrl_renderer_core_integrated
    PRIVATE
    cxx_std_17)

target_compile_definitions(
    dsrrl_renderer_core_integrated
    PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:WIN32_LEAN_AND_MEAN>
    $<$<COMPILE_LANGUAGE:CXX>:NOMINMAX>
    $<$<COMPILE_LANGUAGE:CXX>:DSRRL_CORE_ISLANDS_VERSION="${DSRRL_CORE_ISLANDS_VERSION}">
    $<$<COMPILE_LANGUAGE:CXX>:DSRRL_CORE_ISLANDS_PRODUCT_LINE="DSRRL_Core_Islands">
    $<$<COMPILE_LANGUAGE:CXX>:DSRRL_SOURCE_COMMIT="${DSRRL_SOURCE_COMMIT}">
    $<$<COMPILE_LANGUAGE:CXX>:DSRRL_BUILD_FLAVOR="${DSRRL_BUILD_FLAVOR}">)

if(DSRRL_RELEASE_CLEANUP)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_RELEASE_CLEANUP>)
endif()

if(DSRRL_STUTTER_PROFILE)
    target_compile_definitions(dsrrl_renderer_core PRIVATE DSRRL_STUTTER_PROFILE)
    target_compile_definitions(dsrrl_renderer_core_integrated PRIVATE DSRRL_STUTTER_PROFILE)
endif()
if(DSRRL_STUTTER_HITCH_TRACE)
    if(NOT DSRRL_STUTTER_PROFILE)
        message(FATAL_ERROR "DSRRL_STUTTER_HITCH_TRACE requires DSRRL_STUTTER_PROFILE")
    endif()
    target_compile_definitions(dsrrl_renderer_core PRIVATE DSRRL_STUTTER_HITCH_TRACE)
    target_compile_definitions(dsrrl_renderer_core_integrated PRIVATE DSRRL_STUTTER_HITCH_TRACE)
endif()

if(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
endif()
if(DSRRL_COMPANION_TLS_4WAY)
    if(NOT DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
        message(FATAL_ERROR "Four-way TLS requires keyed epoch synchronization")
    endif()
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE DSRRL_COMPANION_TLS_4WAY)
endif()
if(DSRRL_MR_MATERIAL_TRACE)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE DSRRL_MR_MATERIAL_TRACE)
endif()
if(DSRRL_PMETAL_SPEC_CUT_TRACE)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE DSRRL_PMETAL_SPEC_CUT_TRACE)
endif()

if(DSRRL_DIAGNOSTIC_TELEMETRY_DEFAULT_ON)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_RUNTIME_TELEMETRY_DEFAULT_ON>
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_EFFECT_TELEMETRY_DEFAULT_ON>
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_RUNTIME_VERIFY_STATE_DEFAULT_ON>)
elseif(DSRRL_FULL_TELEMETRY_DEFAULT_ON)
    # Full telemetry is intentionally distinct from full-runtime diagnostics:
    # keep hot/effect counters on, but do not force native D3D state readbacks
    # (PSGetShader/CB/SRV/Sampler) on every draw transaction.
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_RUNTIME_TELEMETRY_DEFAULT_ON>
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_EFFECT_TELEMETRY_DEFAULT_ON>)
elseif(DSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_EFFECT_TELEMETRY_DEFAULT_ON>)
endif()

if(DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_NATIVE_DSR_CUBEMAP_FEED>)
endif()

if(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG>)
endif()

if(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG>)
endif()

if(DSRRL_PMETAL_V13_PTDE_PACKEDGI_MATERIAL_MOD_DIAG)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG>
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FORCE_PTDE_PACKEDGI>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG>
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FORCE_PTDE_PACKEDGI>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R3)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R3>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R4)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R4>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG OR
       NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R3 OR
       NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R4)
        message(FATAL_ERROR
            "P_Metal R5 requires FULL_PTDE_HEMENV_DIAG + R3 + R4 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R5>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
        message(FATAL_ERROR
            "P_Metal R6 requires the complete R5 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R6>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R6)
        message(FATAL_ERROR
            "P_Metal R7 shadow requires the complete R6 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW)
        message(FATAL_ERROR
            "P_Metal R8 PRE A/B requires the complete R7 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB)
        message(FATAL_ERROR
            "P_Metal R11 common merge requires the clean cumulative R8 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE)
        message(FATAL_ERROR
            "P_Metal R15 PTDE live-out pair requires the complete R11 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR)
        message(FATAL_ERROR
            "P_Metal R16 legacy diffuse split requires the cumulative R15B live-out lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT>)
endif()

if(DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT)
        message(FATAL_ERROR
            "P_Metal R17 PTDE diffuse material producer requires the cumulative R16 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL>)
endif()

if(DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL)
        message(FATAL_ERROR
            "P_Metal R19 Lerp MaterialWorkflow requires the canonical cumulative R17 stable lineage as source authority")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW>)
endif()

if(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    if(NOT DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL)
        message(FATAL_ERROR
            "P_Metal R19 exact Lerp EnvDiffuse requires the canonical cumulative R17 lineage")
    endif()

    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE>)
endif()

if(DSRRL_POINTLIGHT_DRAWTIME_BYPASS)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_POINTLIGHT_DRAWTIME_BYPASS>)
endif()

if(DSRRL_DRAWTIME_ISLANDS_BYPASS)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_DRAWTIME_ISLANDS_BYPASS>)
endif()

if(DSRRL_DRAW_CALLBACKS_BYPASS)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_DRAW_CALLBACKS_BYPASS>)
endif()

if(DSRRL_DRAW_REPLAY_BYPASS)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_DRAW_REPLAY_BYPASS>)
endif()

if(DSRRL_EMPTY_DRAW_CALLBACK_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_EMPTY_DRAW_CALLBACK_BISECT>)
endif()

if(DSRRL_STATE_TRANSACTION_ONLY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_STATE_TRANSACTION_ONLY_BISECT>)
endif()

if(DSRRL_RAW_DRAW_REPLAY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_RAW_DRAW_REPLAY_BISECT>)
endif()

if(DSRRL_STATE_CAPTURE_ONLY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_STATE_CAPTURE_ONLY_BISECT>)
endif()

if(DSRRL_NATIVE_STATE_MUTATE_RESTORE_ONLY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_NATIVE_STATE_MUTATE_RESTORE_ONLY_BISECT>)
endif()

if(DSRRL_CORE_TRANSACTION_ONLY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_CORE_TRANSACTION_ONLY_BISECT>)
endif()

if(DSRRL_RAW_NATIVE_DRAW_REENTRY_MIN_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_RAW_NATIVE_DRAW_REENTRY_MIN_BISECT>)
endif()

if(DSRRL_ADDON_LOADED_ONLY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_ADDON_LOADED_ONLY_BISECT>)
endif()

if(DSRRL_DRAW_CALLBACK_ONLY_BISECT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:DSRRL_DRAW_CALLBACK_ONLY_BISECT>)
endif()

if(DSRRL_PMETAL_SRV_SHADOW_EXPERIMENT)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        DSRRL_PMETAL_SRV_SHADOW_EXPERIMENT)
endif()

if(DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE)
endif()

if(DSRRL_FLVER_SELECTOR_PROFILE)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        DSRRL_FLVER_SELECTOR_PROFILE)
endif()

if(DSRRL_POINTLIGHT_PROFILE)
    target_compile_definitions(
        dsrrl_renderer_core_integrated
        PRIVATE
        DSRRL_POINTLIGHT_PROFILE)
endif()

if(MSVC)
    target_compile_options(
        dsrrl_renderer_core_integrated
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/WX>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/Brepro>
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
    target_link_options(
        dsrrl_renderer_core_integrated
        PRIVATE
        /Brepro
        /INCREMENTAL:NO)
endif()

set_target_properties(
    dsrrl_renderer_core_integrated
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${DSRRL_CORE_ISLANDS_ARTIFACT_STEM}"
    SUFFIX ".addon64")

if(DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE)
    add_custom_command(
        TARGET dsrrl_renderer_core_integrated
        POST_BUILD
        COMMAND
            "${Python3_EXECUTABLE}"
            "${DSRRL_RENDERER_CORE_ROOT}/tools/audit_runtime_v2_physical_cut.py"
            --source-dir "${DSRRL_RENDERER_CORE_ROOT}"
            --build-dir "${CMAKE_BINARY_DIR}"
            --post-build
        VERBATIM)
endif()

add_executable(dsrrl_fixed_pointlight_corpus_native_check
    "${DSRRL_RENDERER_CORE_ROOT}/tests/fixed_pointlight_corpus_native_check.cpp")
target_include_directories(dsrrl_fixed_pointlight_corpus_native_check PRIVATE "${DSRRL_RENDERER_CORE_ROOT}/include")
target_compile_features(dsrrl_fixed_pointlight_corpus_native_check PRIVATE cxx_std_17)
target_link_libraries(dsrrl_fixed_pointlight_corpus_native_check PRIVATE dsrrl_renderer_core d3d11)
