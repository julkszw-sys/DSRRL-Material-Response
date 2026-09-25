cmake_minimum_required(VERSION 3.20)
project(DSRRLRendererCoreV1 LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(DSRRL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(DSRRL_NATIVE_PROBE_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/env_spec/generated_native_probe_hash_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_NATIVE_PROBE_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_native_envspec_probe_hash.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/census/DSR_NATIVE_ENVSPEC_PROBE_HASH_V1.bin"
            --output "${DSRRL_NATIVE_PROBE_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_native_envspec_probe_hash.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/census/DSR_NATIVE_ENVSPEC_PROBE_HASH_V1.bin"
    VERBATIM
)
add_custom_target(dsrrl_native_envspec_probe_header
    DEPENDS "${DSRRL_NATIVE_PROBE_HEADER}")

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
    src/operators/material_response/mtd_semantic_census.cpp
    src/operators/env_spec/env_spec_island.cpp
    src/operators/env_spec/no_spc_envspec_delete.cpp
    src/operators/surface/terminal_sat_rgb_patch.cpp
    src/operators/surface/faceeye_shadow.cpp
    src/operators/point_light/pnts_attenuation.cpp
    src/operators/point_light/legacy_specular.cpp
    src/operators/surface/diffuse_material_domain.cpp
    src/operators/surface/fixed_postfog_identity.cpp
    src/operators/lightbank/upper_lower.cpp
    src/operators/lightbank/hemdir3.cpp
    src/operators/resource_bridges/spec_rgb_bridge.cpp
    src/operators/resource_bridges/normal_bridge.cpp
    src/operators/resource_bridges/diffuse_bridge.cpp
    "${DSRRL_NATIVE_PROBE_HEADER}"
)
add_dependencies(dsrrl_renderer_core dsrrl_native_envspec_probe_header)
target_include_directories(dsrrl_renderer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DSRRL_GENERATED_DIR})
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

add_executable(dsrrl_renderer_future_runtime_tests
    tests/future_runtime_readiness_tests.cpp)
target_link_libraries(dsrrl_renderer_future_runtime_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_future_runtime_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_faceeye_pointlight_tests
    tests/faceeye_pointlight_readiness_tests.cpp)
target_link_libraries(dsrrl_renderer_faceeye_pointlight_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_faceeye_pointlight_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_postprocess_unblock_tests
    tests/postprocess_unblock_tests.cpp)
target_link_libraries(dsrrl_renderer_postprocess_unblock_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_postprocess_unblock_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_build151_nospc_tests
    tests/build151_nospc_extension_tests.cpp)
target_link_libraries(dsrrl_renderer_build151_nospc_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_build151_nospc_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_mtd_semantic_census_tests
    tests/mtd_semantic_census_tests.cpp)
target_link_libraries(dsrrl_renderer_mtd_semantic_census_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_mtd_semantic_census_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_integrated_policy_tests
    tests/integrated_feature_policy_tests.cpp)
target_link_libraries(dsrrl_renderer_integrated_policy_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_integrated_policy_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(dsrrl_renderer_core_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_future_runtime_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_faceeye_pointlight_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_postprocess_unblock_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_build151_nospc_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_mtd_semantic_census_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_integrated_policy_tests PRIVATE /W4 /WX)
else()
    target_compile_options(dsrrl_renderer_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_future_runtime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_faceeye_pointlight_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_postprocess_unblock_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_build151_nospc_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_mtd_semantic_census_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_integrated_policy_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
add_test(NAME dsrrl_renderer_future_runtime_tests COMMAND dsrrl_renderer_future_runtime_tests)
add_test(NAME dsrrl_renderer_faceeye_pointlight_tests COMMAND dsrrl_renderer_faceeye_pointlight_tests)
add_test(NAME dsrrl_renderer_postprocess_unblock_tests COMMAND dsrrl_renderer_postprocess_unblock_tests)
add_test(NAME dsrrl_renderer_build151_nospc_tests COMMAND dsrrl_renderer_build151_nospc_tests)
add_test(NAME dsrrl_renderer_mtd_semantic_census_tests COMMAND dsrrl_renderer_mtd_semantic_census_tests)
add_test(NAME dsrrl_renderer_integrated_policy_tests COMMAND dsrrl_renderer_integrated_policy_tests)
