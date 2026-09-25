cmake_minimum_required(VERSION 3.20)

project(DSRRLRendererCoreIntegrated LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DSRRL integrated addon is Windows-only.")
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
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/a1_create_pipeline_bridge.cpp"
    # SpecRGB's exact-identity resource cache is part of the integrated
    # construction target before live resource/draw events are enabled. This
    # keeps the carrier available to the addon without claiming runtime or
    # pixel equivalence; event integration remains separately fail-open.
    "${DSRRL_RENDERER_CORE_ROOT}/src/runtime/spec_rgb_resource_cache.cpp")

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
    NOMINMAX)

if(MSVC)
    target_compile_options(
        dsrrl_renderer_core_integrated
        PRIVATE
        /W4
        /WX
        /EHsc)
endif()

set_target_properties(
    dsrrl_renderer_core_integrated
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DSRRL_Renderer_Core_Integrated_Construction"
    SUFFIX ".addon64")
