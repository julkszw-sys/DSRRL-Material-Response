cmake_minimum_required(VERSION 3.20)

project(DSRRLNexusSourceReview LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The ReShade addon review target is Windows-only.")
endif()

set(RESHade_INCLUDE_DIR "" CACHE PATH "Path to the pinned ReShade include directory")
if(NOT RESHade_INCLUDE_DIR)
    message(FATAL_ERROR "Set -DRESHade_INCLUDE_DIR=PATH/TO/reshade/include")
endif()

add_library(dsrrl_review_core STATIC
    src/runtime_core_v2.cpp
)
target_include_directories(dsrrl_review_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(dsrrl_review_core PUBLIC cxx_std_17)
target_compile_options(dsrrl_review_core PRIVATE /W4 /WX)

add_library(dsrrl_review_reshade OBJECT
    src/runtime_core_v2_reshade.cpp
)
target_include_directories(dsrrl_review_reshade PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${RESHade_INCLUDE_DIR}
)
target_compile_features(dsrrl_review_reshade PRIVATE cxx_std_17)
target_compile_options(dsrrl_review_reshade PRIVATE /W4 /WX)

add_library(dsrrl_nexus_source_review SHARED
    src/runtime_core_v2_diagnostic_addon.cpp
    $<TARGET_OBJECTS:dsrrl_review_reshade>
)
target_include_directories(dsrrl_nexus_source_review PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${RESHade_INCLUDE_DIR}
)
target_link_libraries(dsrrl_nexus_source_review PRIVATE dsrrl_review_core)
target_compile_features(dsrrl_nexus_source_review PRIVATE cxx_std_17)
target_compile_options(dsrrl_nexus_source_review PRIVATE /W4 /WX)
set_target_properties(dsrrl_nexus_source_review PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DSRRL_Nexus_Source_Review"
    SUFFIX ".addon64"
)

add_executable(dsrrl_review_tests tests/runtime_core_v2_tests.cpp)
target_link_libraries(dsrrl_review_tests PRIVATE dsrrl_review_core)
target_compile_features(dsrrl_review_tests PRIVATE cxx_std_17)
target_compile_options(dsrrl_review_tests PRIVATE /W4 /WX)

enable_testing()
add_test(NAME dsrrl_review_tests COMMAND dsrrl_review_tests)
