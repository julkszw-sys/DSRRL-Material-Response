cmake_minimum_required(VERSION 3.20)
project(DSRRL_SPC25_NativeT1Epoch LANGUAGES CXX)

enable_testing()
add_executable(spc25_native_t1_epoch_test
    ../test_native_ps_t1_view_lifetime.cpp)
target_include_directories(spc25_native_t1_epoch_test PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../include")
target_compile_features(spc25_native_t1_epoch_test PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(spc25_native_t1_epoch_test PRIVATE
        /W4 /WX /permissive- /EHsc /Brepro)
else()
    target_compile_options(spc25_native_t1_epoch_test PRIVATE
        -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME spc25_native_t1_epoch_test
         COMMAND spc25_native_t1_epoch_test)
