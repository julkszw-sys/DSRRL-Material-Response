cmake_minimum_required(VERSION 3.20)

project(DSRRLA1CreateTimeDiagnostic LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "A1 create-time diagnostic addon is Windows-only.")
endif()

set(RESHade_INCLUDE_DIR "" CACHE PATH "Path to pinned ReShade include directory")
if(NOT RESHade_INCLUDE_DIR)
    message(FATAL_ERROR "Set -DRESHade_INCLUDE_DIR=PATH/TO/reshade/include")
endif()

get_filename_component(
    DSRRL_RENDERER_CORE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE)

add_library(
    dsrrl_a1_create_time_diagnostic
    SHARED
    a1_create_time_diagnostic_addon.cpp
    "${DSRRL_RENDERER_CORE_ROOT}/src/feature_registry.cpp")

target_include_directories(
    dsrrl_a1_create_time_diagnostic
    PRIVATE
    "${DSRRL_RENDERER_CORE_ROOT}/include"
    "${RESHade_INCLUDE_DIR}")

target_compile_features(
    dsrrl_a1_create_time_diagnostic
    PRIVATE
    cxx_std_17)

target_compile_definitions(
    dsrrl_a1_create_time_diagnostic
    PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

if(MSVC)
    target_compile_options(
        dsrrl_a1_create_time_diagnostic
        PRIVATE
        /W4
        /WX
        /EHsc)
endif()

set_target_properties(
    dsrrl_a1_create_time_diagnostic
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DSRRL_Renderer_Core_A1_CreateTime_Diagnostic"
    SUFFIX ".addon64")
