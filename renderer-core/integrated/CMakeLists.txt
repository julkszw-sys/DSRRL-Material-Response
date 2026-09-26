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

add_custom_target(
    dsrrl_core_upper_lower_authority
    DEPENDS
        "${DSRRL_CORE_GENERATED_LIGHTBANK}/generated_upper_lower_hemenv_v1.hpp")

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
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/pmetal_envspec_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/subsurface_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/subsurface_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/upper_lower_hemenv_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_mode_transport.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_pipeline_registry.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/hemdir3_draw_runtime.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/draw_state_transaction.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/island_draw_adapter.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/material_response_draw_transaction.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/material_response/material_response_v211_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/resource_bridges/spec_rgb_consumer_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/env_spec/pmetal_rgba_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/lightbank/hemdir3_b13_materializer.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/operators/lightbank/upper_lower_hemenv_materializer.cpp")

add_dependencies(
    dsrrl_renderer_core_integrated
    dsrrl_core_material_resource_routes
    dsrrl_core_hemdir3_authority
    dsrrl_core_upper_lower_authority
    dsrrl_core_pmetal_env_source_authority)

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
    $<$<COMPILE_LANGUAGE:CXX>:DSRRL_CORE_ISLANDS_PRODUCT_LINE="DSRRL_Core_Islands">)

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
