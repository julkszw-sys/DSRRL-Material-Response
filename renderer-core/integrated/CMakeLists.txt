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
            --output "${DSRRL_CORE_GENERATED_RUNTIME}/generated_pmetal_env_source_authority.hpp"
    DEPENDS
        "${DSRRL_RENDERER_CORE_ROOT}/tools/generate_pmetal_env_source_authority.py"
        "${DSRRL_RENDERER_CORE_ROOT}/provenance/mr145/SOURCE_V13_PTDE_DONOR.cpp"
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

add_library(
    dsrrl_renderer_core_integrated
    SHARED
    integrated_addon.cpp
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
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_producer_state.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_native_draw_bridge.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pixel_srv_shadow.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemenvlerp_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/subsurface_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/subsurface_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/fixed_pointlight_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/clustered_pnts_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/clustered_pnts_pipeline_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/fixed_pointlight_pipeline_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_hemenv_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_mode_transport.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_draw_runtime.cpp"
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

# Tests the real producer/selector/GPU cache without installing a game hook.
add_executable(dsrrl_fixed_pointlight_transport_windows_tests
    "${DSRRL_RENDERER_CORE_ROOT}/tests/fixed_pointlight_transport_windows_tests.cpp")
target_include_directories(dsrrl_fixed_pointlight_transport_windows_tests PRIVATE
    "${DSRRL_RENDERER_CORE_ROOT}/include" "${RESHADE_INCLUDE_DIR}")
target_compile_features(dsrrl_fixed_pointlight_transport_windows_tests PRIVATE cxx_std_17)
target_link_libraries(dsrrl_fixed_pointlight_transport_windows_tests PRIVATE d3d11)
add_test(NAME dsrrl_fixed_pointlight_transport_windows_tests COMMAND dsrrl_fixed_pointlight_transport_windows_tests)

add_executable(dsrrl_fixed_pointlight_corpus_native_check
    "${DSRRL_RENDERER_CORE_ROOT}/tests/fixed_pointlight_corpus_native_check.cpp")
target_include_directories(dsrrl_fixed_pointlight_corpus_native_check PRIVATE "${DSRRL_RENDERER_CORE_ROOT}/include")
target_compile_features(dsrrl_fixed_pointlight_corpus_native_check PRIVATE cxx_std_17)
target_link_libraries(dsrrl_fixed_pointlight_corpus_native_check PRIVATE dsrrl_renderer_core d3d11)
