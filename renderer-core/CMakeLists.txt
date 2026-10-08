cmake_minimum_required(VERSION 3.20)
project(DSRRLRendererCoreV1 LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

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

set(DSRRL_MTD_IDENTITY_SUPPLEMENT_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/data/census/dsr_mtd_identity_certified_supplement_v1.json")
set(DSRRL_MTD_IDENTITY_SUPPLEMENT_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/material_response/generated_dsr_mtd_identity_supplement_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_MTD_IDENTITY_SUPPLEMENT_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dsr_mtd_identity_supplement.py"
            --input "${DSRRL_MTD_IDENTITY_SUPPLEMENT_SOURCE}"
            --output "${DSRRL_MTD_IDENTITY_SUPPLEMENT_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dsr_mtd_identity_supplement.py"
        "${DSRRL_MTD_IDENTITY_SUPPLEMENT_SOURCE}"
    VERBATIM
)
add_custom_target(dsrrl_dsr_mtd_identity_supplement_header
    DEPENDS "${DSRRL_MTD_IDENTITY_SUPPLEMENT_HEADER}")

set(DSRRL_MR_EXACT_BINDING_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/data/census/ptde_mtd_semantic_exact_bindings_v1.json")
set(DSRRL_MR_EXACT_BINDING_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/material_response/generated_exact_binding_mr_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_MR_EXACT_BINDING_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_material_response_exact_binding_extension_v1.py"
            --bindings "${DSRRL_MR_EXACT_BINDING_SOURCE}"
            --routes "${CMAKE_CURRENT_SOURCE_DIR}/data/material_response_routes_v1.json"
            --donors "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/ptde_material_donor_registry.hpp"
            --output "${DSRRL_MR_EXACT_BINDING_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_material_response_exact_binding_extension_v1.py"
        "${DSRRL_MR_EXACT_BINDING_SOURCE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/material_response_routes_v1.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/ptde_material_donor_registry.hpp"
    VERBATIM
)
add_custom_target(dsrrl_mr_exact_binding_header
    DEPENDS "${DSRRL_MR_EXACT_BINDING_HEADER}")

set(DSRRL_MR_DIFFUSE_V1_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/material_response/generated_diffuse_response_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_MR_DIFFUSE_V1_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_material_response_diffuse_v1.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/material_response_diffuse_v1.tsv"
            --output "${DSRRL_MR_DIFFUSE_V1_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_material_response_diffuse_v1.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/material_response_diffuse_v1.tsv"
    VERBATIM
)
add_custom_target(dsrrl_mr_diffuse_v1_header
    DEPENDS "${DSRRL_MR_DIFFUSE_V1_HEADER}")

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

set(DSRRL_CLUSTERED_PNTS_DIRECT_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/point_light/generated_clustered_pnts_direct_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_CLUSTERED_PNTS_DIRECT_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_clustered_pnts_direct_v1.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/clustered_pnts_direct_stock_journal_v1.json"
            --output "${DSRRL_CLUSTERED_PNTS_DIRECT_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_clustered_pnts_direct_v1.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/clustered_pnts_direct_stock_journal_v1.json"
    VERBATIM
)
add_custom_target(dsrrl_clustered_pnts_direct_header
    DEPENDS "${DSRRL_CLUSTERED_PNTS_DIRECT_HEADER}")

set(DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/point_light/generated_pointlight_material_authority_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_pointlight_material_authority_v1.py"
            --input "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/pointlight_material_authority_v1.json"
            --output "${DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_pointlight_material_authority_v1.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/pointlight_material_authority_v1.json"
    VERBATIM
)
add_custom_target(dsrrl_pointlight_material_authority_header
    DEPENDS "${DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_HEADER}")

set(DSRRL_SUBSURFACE_PLAIN_PATCH_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/subsurface_plain_exact_patches_v1")
set(DSRRL_SUBSURFACE_PLAIN_PATCH_HEADER
    "${DSRRL_GENERATED_DIR}/dsrrl/operators/resource_bridges/generated_subsurface_plain_patches_v1.hpp")
add_custom_command(
    OUTPUT "${DSRRL_SUBSURFACE_PLAIN_PATCH_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_subsurface_plain_exact_patches_v1.py"
            --input-dir "${DSRRL_SUBSURFACE_PLAIN_PATCH_DIR}"
            --output "${DSRRL_SUBSURFACE_PLAIN_PATCH_HEADER}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_subsurface_plain_exact_patches_v1.py"
        "${DSRRL_SUBSURFACE_PLAIN_PATCH_DIR}/Csd.bin.z"
        "${DSRRL_SUBSURFACE_PLAIN_PATCH_DIR}/Sdw.bin.z"
        "${DSRRL_SUBSURFACE_PLAIN_PATCH_DIR}/Plain.bin.z"
    VERBATIM
)
add_custom_target(dsrrl_subsurface_plain_patch_header
    DEPENDS "${DSRRL_SUBSURFACE_PLAIN_PATCH_HEADER}")

option(DSRRL_BUILD_PMETAL_R11_OFFLINE_TEST
    "Build the stock-DXBC R11 common-merge materializer harness (fixtures supplied externally)"
    OFF)
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
    src/operators/dof/dof_island.cpp
    src/operators/point_light/pnts_attenuation.cpp
    src/operators/point_light/clustered_sidecar.cpp
    src/operators/point_light/clustered_pnts_direct_materializer.cpp
    src/operators/point_light/legacy_specular.cpp
    src/operators/point_light/local_specular_receiver_registry.cpp
    src/operators/point_light/local_specular_microfacet_windows.cpp
    src/operators/point_light/fixed_local_specular_patch_plan.cpp
    src/operators/point_light/fixed_local_specular_operand_contract.cpp
    src/operators/point_light/fixed_local_specular_ptde_reference.cpp
    src/operators/point_light/fixed_local_specular_pow_lowering.cpp
    src/operators/point_light/fixed_local_specular_reflect_lowering.cpp
    src/operators/point_light/fixed_local_specular_legacy_kernel.cpp
    src/operators/point_light/fixed_local_specular_output_cut.cpp
    src/operators/point_light/fixed_local_specular_island_plan.cpp
    src/operators/point_light/fixed_local_specular_t19_lowering.cpp
    src/operators/point_light/fixed_local_specular_material_samples.cpp
    src/operators/point_light/fixed_local_geometry_contract.cpp
    src/operators/point_light/fixed_local_specular_single_materializer.cpp
    src/operators/surface/diffuse_material_domain.cpp
    src/operators/surface/fixed_postfog_identity.cpp
    src/operators/lightbank/upper_lower.cpp
    src/operators/lightbank/hemdir3.cpp
    src/operators/resource_bridges/spec_rgb_bridge.cpp
    src/operators/resource_bridges/subsurface_plain_target_materializer.cpp
    src/operators/resource_bridges/normal_bridge.cpp
    src/operators/resource_bridges/diffuse_bridge.cpp
    "${DSRRL_NATIVE_PROBE_HEADER}"
    "${DSRRL_OWNER_CORPUS_HEADER}"
    "${DSRRL_MTD_IDENTITY_SUPPLEMENT_HEADER}"
    "${DSRRL_MR_EXACT_BINDING_HEADER}"
    "${DSRRL_MR_DIFFUSE_V1_HEADER}"
    "${DSRRL_LOCAL_SPECULAR_RECEIVER_HEADER}"
    "${DSRRL_FIXED_LOCAL_SPECULAR_PTDE_REFERENCE_HEADER}"
    "${DSRRL_CLUSTERED_PNTS_DIRECT_HEADER}"
    "${DSRRL_POINTLIGHT_MATERIAL_AUTHORITY_HEADER}"
    "${DSRRL_SUBSURFACE_PLAIN_PATCH_HEADER}"
)
add_custom_target(dsrrl_pointlight_core_source_audit
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_pointlight_core_sources.py"
            --source-dir "${CMAKE_CURRENT_SOURCE_DIR}"
            --cmake "${CMAKE_CURRENT_LIST_FILE}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_pointlight_core_sources.py"
        "${CMAKE_CURRENT_LIST_FILE}"
    VERBATIM
)

add_custom_target(dsrrl_clustered_pnts_activation_source_audit
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_clustered_pnts_activation_sources.py"
            --source-dir "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_clustered_pnts_activation_sources.py"
    VERBATIM
)

add_custom_target(dsrrl_active_islands_hotpath_audit
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_active_islands_hotpath.py"
            --source-dir "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_active_islands_hotpath.py"
    VERBATIM
)

add_dependencies(
    dsrrl_renderer_core
    dsrrl_pointlight_core_source_audit
    dsrrl_clustered_pnts_activation_source_audit
    dsrrl_active_islands_hotpath_audit
    dsrrl_native_envspec_probe_header
    dsrrl_dsr_flver_owner_tuple_header
    dsrrl_dsr_mtd_identity_supplement_header
    dsrrl_mr_exact_binding_header
    dsrrl_mr_diffuse_v1_header
    dsrrl_local_specular_receiver_header
    dsrrl_fixed_local_specular_ptde_reference_header
    dsrrl_clustered_pnts_direct_header
    dsrrl_pointlight_material_authority_header
    dsrrl_subsurface_plain_patch_header)
target_include_directories(dsrrl_renderer_core PUBLIC
    ${DSRRL_GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dsrrl_renderer_core PUBLIC cxx_std_17)

add_executable(dsrrl_renderer_material_owner_tests tests/material_owner_producer_tests.cpp)
add_executable(dsrrl_renderer_material_owner_selection_tests
    tests/material_owner_selection_tests.cpp)
target_link_libraries(dsrrl_renderer_material_owner_selection_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_material_owner_selection_tests PRIVATE cxx_std_17)
add_executable(dsrrl_renderer_exact_material_cache_tests
    tests/exact_material_cache_interleaving_tests.cpp)
target_include_directories(dsrrl_renderer_exact_material_cache_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dsrrl_renderer_exact_material_cache_tests PRIVATE Threads::Threads)
target_compile_features(dsrrl_renderer_exact_material_cache_tests PRIVATE cxx_std_17)
add_executable(dsrrl_renderer_flver_identity_tests tests/flver_identity_registry_tests.cpp)
target_link_libraries(dsrrl_renderer_flver_identity_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_flver_identity_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_dsr_mtd_identity_supplement_tests
    tests/dsr_mtd_identity_supplement_tests.cpp)
target_link_libraries(dsrrl_renderer_dsr_mtd_identity_supplement_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_dsr_mtd_identity_supplement_tests PRIVATE cxx_std_17)
add_dependencies(
    dsrrl_renderer_dsr_mtd_identity_supplement_tests
    dsrrl_dsr_mtd_identity_supplement_header)

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
add_dependencies(
    dsrrl_renderer_material_owner_tests
    dsrrl_dsr_flver_owner_tuple_header
    dsrrl_dsr_mtd_identity_supplement_header)

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
add_dependencies(
    dsrrl_renderer_island_tests
    dsrrl_pointlight_material_authority_header)

add_executable(dsrrl_renderer_future_runtime_tests
    tests/future_runtime_readiness_tests.cpp)
target_link_libraries(dsrrl_renderer_future_runtime_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_future_runtime_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_faceeye_pointlight_tests
    tests/faceeye_pointlight_readiness_tests.cpp)

add_executable(dsrrl_renderer_clustered_sidecar_tests
    tests/clustered_sidecar_tests.cpp)
target_link_libraries(dsrrl_renderer_clustered_sidecar_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_clustered_sidecar_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_clustered_pnts_materializer_tests
    tests/clustered_pnts_direct_materializer_tests.cpp)
target_link_libraries(dsrrl_renderer_clustered_pnts_materializer_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_clustered_pnts_materializer_tests PRIVATE cxx_std_17)
add_dependencies(
    dsrrl_renderer_clustered_pnts_materializer_tests
    dsrrl_clustered_pnts_direct_header)

add_executable(dsrrl_renderer_clustered_pnts_draw_adapter_tests
    tests/clustered_pnts_draw_adapter_tests.cpp)
target_link_libraries(dsrrl_renderer_clustered_pnts_draw_adapter_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_clustered_pnts_draw_adapter_tests PRIVATE cxx_std_17)
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

add_executable(dsrrl_renderer_fixed_local_specular_material_samples_tests
    tests/fixed_local_specular_material_samples_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_specular_material_samples_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_specular_material_samples_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_geometry_tests
    tests/fixed_local_geometry_contract_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_geometry_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_geometry_tests PRIVATE cxx_std_17)

add_executable(dsrrl_renderer_fixed_local_single_materializer_tests
    tests/fixed_local_specular_single_materializer_tests.cpp)
target_link_libraries(dsrrl_renderer_fixed_local_single_materializer_tests PRIVATE dsrrl_renderer_core)
target_compile_features(dsrrl_renderer_fixed_local_single_materializer_tests PRIVATE cxx_std_17)

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
    target_compile_options(dsrrl_renderer_exact_material_cache_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_dsr_mtd_identity_supplement_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_future_runtime_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_faceeye_pointlight_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_clustered_sidecar_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_clustered_pnts_materializer_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_clustered_pnts_draw_adapter_tests PRIVATE /W4 /WX)
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
    target_compile_options(dsrrl_renderer_fixed_local_specular_material_samples_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_geometry_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_single_materializer_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_fixed_local_specular_kernel_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_postprocess_unblock_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_build151_nospc_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_mtd_semantic_census_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_stable_receiver_pipeline_tests PRIVATE /W4 /WX)
    target_compile_options(dsrrl_renderer_hemenvlerp_pipeline_tests PRIVATE /W4 /WX)
else()
    target_compile_options(dsrrl_renderer_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_material_owner_selection_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_exact_material_cache_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_dsr_mtd_identity_supplement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_island_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_future_runtime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_faceeye_pointlight_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_clustered_sidecar_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_clustered_pnts_materializer_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_clustered_pnts_draw_adapter_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
    target_compile_options(dsrrl_renderer_fixed_local_specular_material_samples_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_geometry_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dsrrl_renderer_fixed_local_single_materializer_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
add_test(NAME dsrrl_renderer_exact_material_cache_tests COMMAND dsrrl_renderer_exact_material_cache_tests)
add_test(NAME dsrrl_renderer_flver_identity_tests COMMAND dsrrl_renderer_flver_identity_tests)
add_test(NAME dsrrl_renderer_dsr_mtd_identity_supplement_tests COMMAND dsrrl_renderer_dsr_mtd_identity_supplement_tests)
add_test(NAME dsrrl_renderer_stable_receiver_pipeline_tests COMMAND dsrrl_renderer_stable_receiver_pipeline_tests)
add_test(NAME dsrrl_renderer_hemenvlerp_pipeline_tests COMMAND dsrrl_renderer_hemenvlerp_pipeline_tests)
set_tests_properties(
    dsrrl_renderer_material_owner_selection_tests
    dsrrl_renderer_stable_receiver_pipeline_tests
    dsrrl_renderer_hemenvlerp_pipeline_tests
    PROPERTIES ENVIRONMENT "DSRRL_RUNTIME_TELEMETRY=1")
add_test(NAME dsrrl_renderer_core_tests COMMAND dsrrl_renderer_core_tests)
add_test(NAME dsrrl_renderer_island_tests COMMAND dsrrl_renderer_island_tests)
add_test(NAME dsrrl_renderer_future_runtime_tests COMMAND dsrrl_renderer_future_runtime_tests)
add_test(NAME dsrrl_renderer_faceeye_pointlight_tests COMMAND dsrrl_renderer_faceeye_pointlight_tests)
add_test(NAME dsrrl_renderer_clustered_sidecar_tests COMMAND dsrrl_renderer_clustered_sidecar_tests)
add_test(NAME dsrrl_renderer_clustered_pnts_materializer_tests COMMAND dsrrl_renderer_clustered_pnts_materializer_tests)
add_test(NAME dsrrl_renderer_clustered_pnts_draw_adapter_tests COMMAND dsrrl_renderer_clustered_pnts_draw_adapter_tests)
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
add_test(NAME dsrrl_renderer_fixed_local_specular_material_samples_tests COMMAND dsrrl_renderer_fixed_local_specular_material_samples_tests)
add_test(NAME dsrrl_renderer_fixed_local_geometry_tests COMMAND dsrrl_renderer_fixed_local_geometry_tests)
add_test(NAME dsrrl_renderer_fixed_local_single_materializer_tests COMMAND dsrrl_renderer_fixed_local_single_materializer_tests)
add_test(NAME dsrrl_renderer_fixed_local_specular_kernel_tests COMMAND dsrrl_renderer_fixed_local_specular_kernel_tests)
add_test(NAME dsrrl_renderer_postprocess_unblock_tests COMMAND dsrrl_renderer_postprocess_unblock_tests)
add_test(NAME dsrrl_renderer_build151_nospc_tests COMMAND dsrrl_renderer_build151_nospc_tests)
add_test(NAME dsrrl_renderer_mtd_semantic_census_tests COMMAND dsrrl_renderer_mtd_semantic_census_tests)
add_test(
    NAME dsrrl_material_response_specular_power_donor_audit
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/audit_material_response_specular_power_donors_v1.py"
            --routes "${CMAKE_CURRENT_SOURCE_DIR}/include/dsrrl/operators/material_response/generated_routes_v1.hpp"
            --donors "${CMAKE_CURRENT_SOURCE_DIR}/data/provenance/ptde_material_donor_registry.hpp"
            --generated "${CMAKE_CURRENT_SOURCE_DIR}/include/dsrrl/operators/material_response/generated_material_constants_v1.hpp"
)

add_executable(dsrrl_pmetal_selector_policy_tests tests/pmetal_selector_policy_tests.cpp)
target_include_directories(dsrrl_pmetal_selector_policy_tests PRIVATE include)
target_compile_features(dsrrl_pmetal_selector_policy_tests PRIVATE cxx_std_17)
add_test(NAME dsrrl_pmetal_selector_policy_tests COMMAND dsrrl_pmetal_selector_policy_tests)

add_executable(
    dsrrl_pmetal_producer_state_tests
    tests/pmetal_producer_state_tests.cpp
    src/runtime/pmetal_producer_state.cpp)
target_include_directories(dsrrl_pmetal_producer_state_tests PRIVATE include)
target_compile_features(dsrrl_pmetal_producer_state_tests PRIVATE cxx_std_17)
add_test(NAME dsrrl_pmetal_producer_state_tests COMMAND dsrrl_pmetal_producer_state_tests)

add_executable(dsrrl_pointlight_ptde_source_tests tests/pointlight_ptde_source_tests.cpp)
target_include_directories(dsrrl_pointlight_ptde_source_tests PRIVATE include)
target_compile_features(dsrrl_pointlight_ptde_source_tests PRIVATE cxx_std_17)
add_test(NAME dsrrl_pointlight_ptde_source_tests COMMAND dsrrl_pointlight_ptde_source_tests)

if(DSRRL_BUILD_PMETAL_R11_OFFLINE_TEST)
    add_executable(
        dsrrl_pmetal_r11_offline_test
        tests/pmetal_r11_materializer_test.cpp
        src/operators/material_response/material_response_diffuse_v1.cpp
        src/operators/env_spec/pmetal_rgba_materializer.cpp)

    target_link_libraries(
        dsrrl_pmetal_r11_offline_test
        PRIVATE
        dsrrl_renderer_core)

    target_compile_features(
        dsrrl_pmetal_r11_offline_test
        PRIVATE
        cxx_std_17)

    target_compile_definitions(
        dsrrl_pmetal_r11_offline_test
        PRIVATE
        DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG
        DSRRL_PMETAL_FORCE_PTDE_PACKEDGI
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R3
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R4
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R5
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R6
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB
        DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE)

    add_dependencies(
        dsrrl_pmetal_r11_offline_test
        dsrrl_mr_diffuse_v1_header)

    if(MSVC)
        target_compile_options(
            dsrrl_pmetal_r11_offline_test
            PRIVATE /W4 /WX)
    else()
        target_compile_options(
            dsrrl_pmetal_r11_offline_test
            PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-function)
    endif()
endif()
