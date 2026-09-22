cmake_minimum_required(VERSION 3.20)

project(DSRRLRuntimeCoreV2 LANGUAGES CXX)

add_library(dsrrl_runtime_core_v2 STATIC
    src/runtime_core_v2.cpp
)

target_include_directories(dsrrl_runtime_core_v2
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(dsrrl_runtime_core_v2 PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(dsrrl_runtime_core_v2 PRIVATE /W4 /WX)
else()
    target_compile_options(dsrrl_runtime_core_v2 PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(dsrrl_runtime_core_v2_tests
    tests/runtime_core_v2_tests.cpp
)

target_link_libraries(dsrrl_runtime_core_v2_tests
    PRIVATE dsrrl_runtime_core_v2
)

enable_testing()
add_test(NAME runtime_core_v2 COMMAND dsrrl_runtime_core_v2_tests)

set(RESHADE_INCLUDE_DIR "" CACHE PATH "Path to the ReShade include directory")
# Backward compatibility with the historical mixed-case cache variable.
if(NOT RESHADE_INCLUDE_DIR AND DEFINED RESHade_INCLUDE_DIR AND RESHade_INCLUDE_DIR)
    set(RESHADE_INCLUDE_DIR "${RESHade_INCLUDE_DIR}")
endif()

if(RESHADE_INCLUDE_DIR)
    add_library(dsrrl_runtime_core_v2_reshade OBJECT
        src/runtime_core_v2_reshade.cpp
    )

    target_include_directories(dsrrl_runtime_core_v2_reshade
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${RESHADE_INCLUDE_DIR}
    )

    target_compile_features(dsrrl_runtime_core_v2_reshade PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(dsrrl_runtime_core_v2_reshade PRIVATE /W4 /WX)
    else()
        target_compile_options(dsrrl_runtime_core_v2_reshade PRIVATE -Wall -Wextra -Werror)
    endif()
    if(WIN32)
        add_library(dsrrl_runtime_core_v2_diagnostic SHARED
            src/runtime_core_v2_diagnostic_addon.cpp
            $<TARGET_OBJECTS:dsrrl_runtime_core_v2_reshade>
        )

        target_include_directories(dsrrl_runtime_core_v2_diagnostic
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${RESHADE_INCLUDE_DIR}
        )

        target_link_libraries(dsrrl_runtime_core_v2_diagnostic
            PRIVATE dsrrl_runtime_core_v2
        )

        target_compile_features(dsrrl_runtime_core_v2_diagnostic PRIVATE cxx_std_17)
        target_compile_options(dsrrl_runtime_core_v2_diagnostic PRIVATE /W4 /WX)

        set_target_properties(dsrrl_runtime_core_v2_diagnostic PROPERTIES
            PREFIX ""
            OUTPUT_NAME "DSRRL_Runtime_Core_V2_Diagnostic"
            SUFFIX ".addon64"
        )

        add_library(dsrrl_runtime_core_v2_virtual_envspec SHARED
            src/runtime_core_v2_virtual_envspec.cpp
            $<TARGET_OBJECTS:dsrrl_runtime_core_v2_reshade>
        )

        target_include_directories(dsrrl_runtime_core_v2_virtual_envspec
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${RESHADE_INCLUDE_DIR}
        )

        target_link_libraries(dsrrl_runtime_core_v2_virtual_envspec
            PRIVATE
                dsrrl_runtime_core_v2
                bcrypt
        )

        target_compile_features(dsrrl_runtime_core_v2_virtual_envspec PRIVATE cxx_std_17)
        target_compile_options(dsrrl_runtime_core_v2_virtual_envspec PRIVATE /W4 /WX)

        set_target_properties(dsrrl_runtime_core_v2_virtual_envspec PROPERTIES
            PREFIX ""
            OUTPUT_NAME "DSRRL_PTDE_Virtual_EnvSpec_V1"
            SUFFIX ".addon64"
        )

        add_library(dsrrl_runtime_core_v2_virtual_envspec_v2 SHARED
            src/runtime_core_v2_virtual_envspec_v2.cpp
            $<TARGET_OBJECTS:dsrrl_runtime_core_v2_reshade>
        )

        target_include_directories(dsrrl_runtime_core_v2_virtual_envspec_v2
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${RESHADE_INCLUDE_DIR}
        )

        target_link_libraries(dsrrl_runtime_core_v2_virtual_envspec_v2
            PRIVATE
                dsrrl_runtime_core_v2
                bcrypt
        )

        target_compile_features(dsrrl_runtime_core_v2_virtual_envspec_v2 PRIVATE cxx_std_17)
        target_compile_options(dsrrl_runtime_core_v2_virtual_envspec_v2 PRIVATE /W4 /WX)

        set_target_properties(dsrrl_runtime_core_v2_virtual_envspec_v2 PROPERTIES
            PREFIX ""
            OUTPUT_NAME "DSRRL_PTDE_Virtual_EnvSpec_V2"
            SUFFIX ".addon64"
        )

        add_library(dsrrl_runtime_core_v2_virtual_envspec_v3 SHARED
            src/runtime_core_v2_virtual_envspec_v3.cpp
            $<TARGET_OBJECTS:dsrrl_runtime_core_v2_reshade>
        )

        target_include_directories(dsrrl_runtime_core_v2_virtual_envspec_v3
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${RESHADE_INCLUDE_DIR}
        )

        target_link_libraries(dsrrl_runtime_core_v2_virtual_envspec_v3
            PRIVATE
                dsrrl_runtime_core_v2
                bcrypt
        )

        target_compile_features(dsrrl_runtime_core_v2_virtual_envspec_v3 PRIVATE cxx_std_17)
        target_compile_options(dsrrl_runtime_core_v2_virtual_envspec_v3 PRIVATE /W4 /WX)

        set_target_properties(dsrrl_runtime_core_v2_virtual_envspec_v3 PROPERTIES
            PREFIX ""
            OUTPUT_NAME "DSRRL_PTDE_Virtual_EnvSpec_V3"
            SUFFIX ".addon64"
        )
    endif()

endif()
