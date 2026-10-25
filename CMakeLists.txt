cmake_minimum_required(VERSION 3.21)

project(
    MonitorDisplaySelector
    VERSION 0.2.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
    CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

list(
    PREPEND CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows-static"
)

add_subdirectory(CommonLibSSE)


# ============================================================================
# SKSE PLUGIN
# ============================================================================

add_library(
    MonitorDisplaySelector
    SHARED
    MonitorDisplaySelector.cpp
)

target_link_libraries(
    MonitorDisplaySelector
    PRIVATE
    CommonLibSSE::CommonLibSSE
)

target_compile_definitions(
    MonitorDisplaySelector
    PRIVATE
    _UNICODE
    UNICODE
)

if(MSVC)
    target_compile_options(
        MonitorDisplaySelector
        PRIVATE
        /permissive-
        /Zc:preprocessor
    )
endif()

set_target_properties(
    MonitorDisplaySelector
    PROPERTIES
    OUTPUT_NAME "MonitorDisplaySelector"
)


# ============================================================================
# MONITOR CONFIGURATOR
# ============================================================================

add_executable(
    MonitorDisplaySelectorConfig
    WIN32
    MonitorDisplaySelectorConfig.cpp
)

target_compile_definitions(
    MonitorDisplaySelectorConfig
    PRIVATE
    _UNICODE
    UNICODE
)

set_target_properties(
    MonitorDisplaySelectorConfig
    PROPERTIES
    OUTPUT_NAME "MonitorDisplaySelectorConfig"
)