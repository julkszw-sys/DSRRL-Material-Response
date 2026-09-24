cmake_minimum_required(VERSION 3.20)
project(DSRRLRendererCoreV1 LANGUAGES CXX)

option(DSRRL_BUILD_PHASE0_PROBE "Build the pixel-inert ReShade API20 Phase 0 probe addon" OFF)
set(RESHade_INCLUDE_DIR "" CACHE PATH "Path to pinned ReShade include directory")

add_library(dsrrl_renderer_core STATIC
    src/feature_registry.cpp
    src/hook_registry.cpp
    src/receiver_registry.cpp
    src/snapshot_bus.cpp
    src/shader_registry.cpp
    src/draw_transaction.cpp
    src/renderer_core.cpp
    src/operator_catalog.cpp
    src/island_policy.cpp
    src/operators/legacy_plan/a1_mask_decomposition.cpp
    src/operators/material_response/material_response_island.cpp
    src/operators/material_response/material_response_seed.cpp
    src/operators/env_spec/env_spec_island.cpp
)
target_include_directories(dsrrl_renderer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dsrrl_renderer_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(dsrrl_renderer_core PRIVATE /W4 /WX)
else()
    target_compile_options(dsrrl_renderer_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(dsrrl_renderer_core_tests tests/core_phase0_tests.cpp)
target_link_libraries(dsrrl_renderer_core_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_core_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_island_tests tests/islands_phase1_tests.cpp)
target_link_libraries(dsrrl_renderer_island_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_island_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(dsrrl_renderer_core_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE /W4 /WX)
else()
    target_compile_options(dsrrl_renderer_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(DSRRL_BUILD_PHASE0_PROBE)
    if(NOT WIN32)
        message(FATAL_ERROR "Phase 0 ReShade probe is Windows-only.")
    endif()
    if(NOT RESHade_INCLUDE_DIR)
        message(FATAL_ERROR "Set -DRESHade_INCLUDE_DIR=PATH/TO/reshade/include")
    endif()

    add_library(dsrrl_renderer_core_phase0_probe SHARED
        src/phase0_probe_addon.cpp
    )
    target_include_directories(dsrrl_renderer_core_phase0_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${RESHade_INCLUDE_DIR}
    )
    target_link_libraries(dsrrl_renderer_core_phase0_probe PRIVATE dsrrl_renderer_core)
    target_compile_features(dsrrl_renderer_core_phase0_probe PRIVATE cxx_std_17)
    target_compile_options(dsrrl_renderer_core_phase0_probe PRIVATE /W4 /WX)
    set_target_properties(dsrrl_renderer_core_phase0_probe PROPERTIES
        PREFIX ""
        OUTPUT_NAME "DSRRL_Renderer_Core_Phase0_Probe"
        SUFFIX ".addon64"
    )
endif()

enable_testing()
add_test(NAME dsrrl_renderer_core_tests COMMAND dsrrl_renderer_core_tests)
add_test(NAME dsrrl_renderer_island_tests COMMAND dsrrl_renderer_island_tests)
