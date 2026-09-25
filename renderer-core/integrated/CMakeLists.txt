cmake_minimum_required(VERSION 3.20)

project(DSRRLCoreIslands LANGUAGES CXX ASM_MASM)

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
    flver_selector_hook.asm
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/a1_create_pipeline_bridge.cpp"
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/flver_engine_hooks.cpp")

target_include_directories(
    dsrrl_renderer_core_integrated
    PRIVATE
    "${DSRRL_RENDERER_CORE_ROOT}/include"
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
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    DSRRL_CORE_ISLANDS_VERSION="${DSRRL_CORE_ISLANDS_VERSION}"
    DSRRL_CORE_ISLANDS_PRODUCT_LINE="${DSRRL_CORE_ISLANDS_PRODUCT_LINE}")

if(MSVC)
    target_compile_options(
        dsrrl_renderer_core_integrated
        PRIVATE
        /W4
        /WX
        /permissive-
        /Brepro
        /EHsc)
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
