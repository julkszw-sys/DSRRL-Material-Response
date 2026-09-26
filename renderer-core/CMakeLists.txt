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

set(DSRRL_OWNER_CORPUS_PAYLOAD
    "${CMAKE_CURRENT_SOURCE_DIR}/data/census/dsr_flver_owner_tuple_corpus_v1.bin.z")
set(DSRRL_OWNER_CORPUS_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_OWNER_CORPUS_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dsr_flver_owner_tuple_header.py"
            --input "${DSRRL_OWNER_CORPUS_PAYLOAD}"
            --output "${DSRRL_OWNER_CORPUS_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dsr_flver_owner_tuple_header.py"
        "${DSRRL_OWNER_CORPUS_PAYLOAD}"
    VERBATIM
)
add_custom_target(dsrrl_dsr_flver_owner_tuple_header
    DEPENDS "${DSRRL_OWNER_CORPUS_HEADER}")

set(DSRRL_HEMENVLERP_V211_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/material_response/generated_hemenvlerp_v211_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_HEMENVLERP_V211_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_hemenvlerp_v211_v1.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/hemenvlerp_mr_v211_v1.tsv"
            --output "${DSRRL_HEMENVLERP_V211_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_hemenvlerp_v211_v1.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/hemenvlerp_mr_v211_v1.tsv"
    VERBATIM
)
add_custom_target(dsrrl_hemenvlerp_v211_header
    DEPENDS "${DSRRL_HEMENVLERP_V211_HEADER}")

set(DSRRL_LOCAL_SPECULAR_RECEIVER_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/point_light/generated_local_specular_receivers_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_LOCAL_SPECULAR_RECEIVER_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_local_specular_receivers_v1.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/local_specular_receivers_v1.tsv"
            --output "${DSRRL_LOCAL_SPECULAR_RECEIVER_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_local_specular_receivers_v1.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/local_specular_receivers_v1.tsv"
    VERBATIM
)
add_custom_target(dsrrl_local_specular_receiver_header
    DEPENDS "${DSRRL_LOCAL_SPECULAR_RECEIVER_HEADER}")

set(DSRRL_FIXED_LOCAL_SPECULAR_PTDE_REFERENCE_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/point_light/generated_fixed_local_specular_ptde_reference_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_FIXED_LOCAL_SPECULAR_PTDE_REFERENCE_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_fixed_local_specular_ptde_reference_v1.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/fixed_local_specular_ptde_pairs_v1.tsv"
            --output "${DSRRL_FIXED_LOCAL_SPECULAR_PTDE_REFERENCE_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_fixed_local_specular_ptde_reference_v1.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/fixed_local_specular_ptde_pairs_v1.tsv"
    VERBATIM
)
add_custom_target(dsrrl_fixed_local_specular_ptde_reference_header
    DEPENDS "${DSRRL_FIXED_LOCAL_SPECULAR_PTDE_REFERENCE_HEADER}")

option(DSRRL_BUILD_PHASE0_PROBE "Build the pixel-inert ReShade API20 Phase 0 probe addon" OFF)
set(RESHade_INCLUDE_DIR "" CACHE PATH "Path to pinned ReShade include directory")

add_library(dsrrl_renderer_core STATIC
    src/feature_registry.cpp
    src/hook_registry.cpp
    src/receiver_registry.cpp
    src/snapshot_bus.cpp
    src/shader_registry.cpp
    src/draw_transaction.cpp
    src/runtime/material_owner_producer.cpp
    src/runtime/material_owner_selection.cpp
    src/runtime/flver_identity_registry.cpp
    src/runtime/stable_receiver_pipeline_registry.cpp
    src/runtime/hemenvlerp_pipeline_registry.cpp
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
    src/operators/point_light/local_specular_receiver_registry.cpp
    src/operators/point_light/local_specular_microfacet_windows.cpp
    src/operators/point_light/fixed_local_specular_patch_plan.cpp
    src/operators/point_light/fixed_local_specular_operand_contract.cpp
    src/operators/point_light/fixed_local_specular_ptde_reference.cpp
    src/operators/point_light/fixed_local_specular_pow_lowering.cpp
    src/operators/point_light/fixed_local_specular_reflect_lowering.cpp
    src/operators/point_light/fixed_local_specular_legacy_kernel.cpp
    src/operators/surface/diffuse_material_domain.cpp
    src/operators/surface/fixed_postfog_identity.cpp
    src/operators/lightbank/upper_lower.cpp
    src/operators/lightbank/hemdir3.cpp
    src/operators/resource_bridges/spec_rgb_bridge.cpp
    src/operators/resource_bridges/normal_bridge.cpp
    src/operators/resource_bridges/diffuse_bridge.cpp
    "${DSRRL_NATIVE_PROBE_HEADER}"
    "${DSRRL_OWNER_CORPUS_HEADER}"
    "${DSRRL_LOCAL_SPECULAR_RECEIVER_HEADER}"
    "${DSRRL_FIXED_LOCAL_SPECULAR_PTDE_REFERENCE_HEADER}"
)
add_dependencies(
    dsrrl_renderer_core
    dsrrl_native_envspec_probe_header
    dsrrl_dsr_flver_owner_tuple_header
    dsrrl_hemenvlerp_v211_header
    dsrrl_local_specular_receiver_header
    dsrrl_fixed_local_specular_ptde_reference_header)
target_include_directories(dsrrl_renderer_core PUBLIC
    ${DSRRL_GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dsrrl_renderer_core PUBLIC cxx_std_17)

add_executable(dsrrl_renderer_material_owner_tests tests/material_owner_producer_tests.cpp)
add_executable(dsrrl_renderer_material_owner_selection_tests
    tests/material_owner_selection_tests.cpp)
target_link_libraries(dsrrl_renderer_material_owner_selection_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_material_owner_selection_tests PRIVATE cxx_std_17)
add_executable(dsrrl_renderer_flver_identity_tests tests/flver_identity_registry_tests.cpp)
target_link_libraries(dsrrl_renderer_flver_identity_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_flver_identity_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_stable_receiver_pipeline_tests
    tests/stable_receiver_pipeline_registry_tests.cpp)
target_link_libraries(dsrrl_renderer_stable_receiver_pipeline_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_stable_receiver_pipeline_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_hemenvlerp_pipeline_tests
    tests/hemenvlerp_pipeline_registry_tests.cpp)
target_link_libraries(dsrrl_renderer_hemenvlerp_pipeline_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_hemenvlerp_pipeline_tests PRIVATE cxx_std_17)

target_link_libraries(dsrrl_renderer_material_owner_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_material_owner_tests PRIVATE cxx_std_17)

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

add_executable(dsrrl_renderer_local_specular_receiver_tests
    tests/local_specular_receiver_registry_tests.cpp)
target_link_libraries(dsrrl_renderer_local_specular_receiver_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_local_specular_receiver_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_local_specular_window_tests
    tests/local_specular_microfacet_window_tests.cpp)
target_link_libraries(dsrrl_renderer_local_specular_window_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_local_specular_window_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_plan_tests
    tests/fixed_local_specular_patch_plan_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_plan_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_plan_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_operand_tests
    tests/fixed_local_specular_operand_contract_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_operand_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_operand_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_ptde_reference_tests
    tests/fixed_local_specular_ptde_reference_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_ptde_reference_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_ptde_reference_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_pow_tests
    tests/fixed_local_specular_pow_lowering_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_pow_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_pow_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_reflect_tests
    tests/fixed_local_specular_reflect_lowering_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_reflect_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_reflect_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_output_cut_tests
    tests/fixed_local_specular_output_cut_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_output_cut_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_output_cut_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_island_plan_tests
    tests/fixed_local_specular_island_plan_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_island_plan_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_island_plan_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_t19_tests
    tests/fixed_local_specular_t19_lowering_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_t19_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_t19_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_specular_kernel_tests
    tests/fixed_local_specular_legacy_kernel_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_kernel_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_kernel_tests PRIVATE cxx_std_17)

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

if(MSVC)
    target_compile_options(dsrrl_renderer_core_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_material_owner_selection_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_future_runtime_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_faceeye_pointlight_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_local_specular_receiver_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_local_specular_window_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_plan_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_operand_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_ptde_reference_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_pow_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_reflect_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_output_cut_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_island_plan_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_t19_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_kernel_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_postprocess_unblock_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_build151_nospc_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_mtd_semantic_census_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_stable_receiver_pipeline_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_hemenvlerp_pipeline_tests PRIVATE /W4 /WX)
else()
    target_compile_options(dsrrl_renderer_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_material_owner_selection_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_future_runtime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_faceeye_pointlight_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_local_specular_receiver_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_local_specular_window_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_plan_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_operand_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_ptde_reference_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_pow_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_reflect_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_output_cut_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_island_plan_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_t19_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_specular_kernel_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_postprocess_unblock_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_build151_nospc_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_mtd_semantic_census_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_stable_receiver_pipeline_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_hemenvlerp_pipeline_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
add_test(NAME dsrrl_renderer_material_owner_tests COMMAND dsrrl_renderer_material_owner_tests)
add_test(NAME dsrrl_renderer_material_owner_selection_tests COMMAND dsrrl_renderer_material_owner_selection_tests)
add_test(NAME dsrrl_renderer_flver_identity_tests COMMAND dsrrl_renderer_flver_identity_tests)
add_test(NAME dsrrl_renderer_stable_receiver_pipeline_tests COMMAND dsrrl_renderer_stable_receiver_pipeline_tests)
add_test(NAME dsrrl_renderer_hemenvlerp_pipeline_tests COMMAND dsrrl_renderer_hemenvlerp_pipeline_tests)
add_test(NAME dsrrl_renderer_core_tests COMMAND dsrrl_renderer_core_tests)
add_test(NAME dsrrl_renderer_island_tests COMMAND dsrrl_renderer_island_tests)
add_test(NAME dsrrl_renderer_future_runtime_tests COMMAND dsrrl_renderer_future_runtime_tests)
add_test(NAME dsrrl_renderer_faceeye_pointlight_tests COMMAND dsrrl_renderer_faceeye_pointlight_tests)
add_test(NAME dsrrl_renderer_local_specular_receiver_tests COMMAND dsrrl_renderer_local_specular_receiver_tests)
add_test(NAME dsrrl_renderer_local_specular_window_tests COMMAND dsrrl_renderer_local_specular_window_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_plan_tests COMMAND dsrrl_renderer_fixed_local_specular_plan_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_operand_tests COMMAND dsrrl_renderer_fixed_local_specular_operand_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_ptde_reference_tests COMMAND dsrrl_renderer_fixed_local_specular_ptde_reference_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_pow_tests COMMAND dsrrl_renderer_fixed_local_specular_pow_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_reflect_tests COMMAND dsrrl_renderer_fixed_local_specular_reflect_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_output_cut_tests COMMAND dsrrl_renderer_fixed_local_specular_output_cut_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_island_plan_tests COMMAND dsrrl_renderer_fixed_local_specular_island_plan_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_t19_tests COMMAND dsrrl_renderer_fixed_local_specular_t19_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_kernel_tests COMMAND dsrrl_renderer_fixed_local_specular_kernel_tests)
add_test(NAME dsrrl_renderer_postprocess_unblock_tests COMMAND dsrrl_renderer_postprocess_unblock_tests)
add_test(NAME dsrrl_renderer_build151_nospc_tests COMMAND dsrrl_renderer_build151_nospc_tests)
add_test(NAME dsrrl_renderer_mtd_semantic_census_tests COMMAND dsrrl_renderer_mtd_semantic_census_tests)
