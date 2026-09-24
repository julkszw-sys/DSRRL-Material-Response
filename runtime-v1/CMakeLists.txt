cmake_minimum_required(VERSION 3.20)
project(DSRRLRendererRuntimeV1 LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PROV "${CMAKE_CURRENT_SOURCE_DIR}/provenance/mr_v211")
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")

add_custom_command(
  OUTPUT "${GEN_DIR}/mr_runtime_manifest.hpp"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_mr_runtime_manifest.py"
          --v29 "${PROV}/V29_SHADER_MANIFEST.json"
          --v210 "${PROV}/V210_C101_MATERIAL_RESPONSE_AUDIT.json"
          --v211 "${PROV}/V211_C101_UNBOUNDED_AUDIT.json"
          --out "${GEN_DIR}/mr_runtime_manifest.hpp"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_mr_runtime_manifest.py"
    "${PROV}/V29_SHADER_MANIFEST.json"
    "${PROV}/V210_C101_MATERIAL_RESPONSE_AUDIT.json"
    "${PROV}/V211_C101_UNBOUNDED_AUDIT.json"
  VERBATIM
)

add_custom_target(dsrrl_mr_manifest DEPENDS "${GEN_DIR}/mr_runtime_manifest.hpp")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/A3RouteHeaders.cmake")

add_library(dsrrl_runtime_v1 STATIC
  src/sha256.cpp
  src/mr_dxbc_transform.cpp
  "${GEN_DIR}/mr_runtime_manifest.hpp"
)
add_dependencies(dsrrl_runtime_v1 dsrrl_mr_manifest)
target_include_directories(dsrrl_runtime_v1 PUBLIC include "${GEN_DIR}")
target_compile_features(dsrrl_runtime_v1 PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(dsrrl_runtime_v1 PRIVATE /W4 /WX)
else()
  target_compile_options(dsrrl_runtime_v1 PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(dsrrl_runtime_v1_tests tests/mr_transform_tests.cpp)
target_link_libraries(dsrrl_runtime_v1_tests PRIVATE dsrrl_runtime_v1)
target_compile_features(dsrrl_runtime_v1_tests PRIVATE cxx_std_20)

add_executable(dsrrl_runtime_v1_asset_route_tests tests/asset_route_tests.cpp)
target_include_directories(dsrrl_runtime_v1_asset_route_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/include"
)
target_compile_features(dsrrl_runtime_v1_asset_route_tests PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(dsrrl_runtime_v1_tests PRIVATE /W4 /WX)
  target_compile_options(dsrrl_runtime_v1_asset_route_tests PRIVATE /W4 /WX)
else()
  target_compile_options(dsrrl_runtime_v1_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(dsrrl_runtime_v1_asset_route_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME dsrrl_runtime_v1_tests COMMAND dsrrl_runtime_v1_tests)
add_test(NAME dsrrl_runtime_v1_asset_route_tests COMMAND dsrrl_runtime_v1_asset_route_tests)

set(RESHade_INCLUDE_DIR "" CACHE PATH "Path to pinned ReShade include directory")

if(WIN32 AND RESHade_INCLUDE_DIR)
  enable_language(ASM_MASM)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core"
                   "${CMAKE_CURRENT_BINARY_DIR}/renderer-core")

  add_library(dsrrl_renderer_runtime_v1_addon SHARED
    src/addon.cpp
    src/engine_hooks.cpp
    src/mr_island.cpp
    src/asset_bridges.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/src/runtime/a1_create_pipeline_bridge.cpp"
    src/selector_hook.asm
    src/texture_name_hook.asm
  )
  add_dependencies(dsrrl_renderer_runtime_v1_addon dsrrl_mr_manifest)
  target_include_directories(dsrrl_renderer_runtime_v1_addon PRIVATE
    include
    "${GEN_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/provenance/mr_v211"
    "${RESHade_INCLUDE_DIR}"
  )
  target_link_libraries(dsrrl_renderer_runtime_v1_addon PRIVATE
    dsrrl_runtime_v1
    dsrrl_renderer_core
  )
  target_compile_features(dsrrl_renderer_runtime_v1_addon PRIVATE cxx_std_20)

  # C++ warning/conformance switches must never leak into ml64/MASM.
  # MASM receives its own defaults; /WX remains C++-only here so the
  # mixed-language addon can build deterministically on Windows runners.
  if(MSVC)
    target_compile_options(dsrrl_renderer_runtime_v1_addon PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:/W4>
      $<$<COMPILE_LANGUAGE:CXX>:/WX>
      $<$<COMPILE_LANGUAGE:CXX>:/permissive->
      $<$<COMPILE_LANGUAGE:CXX>:/Brepro>
    )
    target_link_options(dsrrl_renderer_runtime_v1_addon PRIVATE
      /Brepro
      /INCREMENTAL:NO
    )
  endif()

  set_target_properties(dsrrl_renderer_runtime_v1_addon PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DSRRL_Renderer_Core_Runtime_v1"
    SUFFIX ".addon64"
  )

  add_custom_command(TARGET dsrrl_renderer_runtime_v1_addon POST_BUILD
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/canonicalize_pe.py"
            "$<TARGET_FILE:dsrrl_renderer_runtime_v1_addon>"
    COMMENT "Canonicalizing PE metadata for reproducible addon bytes"
    VERBATIM
  )
endif()
