cmake_minimum_required(VERSION 3.24)
project(dsrrl_pmetal_cb_reset_stack_smoke LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Requires Windows: test explicitly targets default Win thread stack")
endif()
add_executable(dsrrl_pmetal_cb_reset_stack_smoke
    ../pmetal_cb_reset_stack_smoke.cpp
    ../../src/runtime/pmetal_native_cb_writer_trace.cpp)
target_include_directories(dsrrl_pmetal_cb_reset_stack_smoke PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../include")
target_compile_features(dsrrl_pmetal_cb_reset_stack_smoke PRIVATE cxx_std_17)
target_compile_definitions(dsrrl_pmetal_cb_reset_stack_smoke PRIVATE
    DSRRL_PMETAL_ASYLUM_CB_REGISTER_TRACE)
if(MSVC)
    target_link_options(dsrrl_pmetal_cb_reset_stack_smoke PRIVATE "/STACK:1048576")
endif()
enable_testing()
add_test(NAME dsrrl_pmetal_cb_reset_stack_smoke COMMAND dsrrl_pmetal_cb_reset_stack_smoke)
