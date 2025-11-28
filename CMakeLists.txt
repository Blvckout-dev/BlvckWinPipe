cmake_minimum_required(VERSION 3.20)
project(
    BlvckWinPipe
    VERSION 0.1.0
    LANGUAGES CXX
)

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_OUT_DIR "${PROJECT_SOURCE_DIR}/lib")
set(BIN_OUT_DIR "${PROJECT_SOURCE_DIR}/bin")

# Options
option(BLVCKWINPIPE_BUILD_SHARED "Build ${PROJECT_NAME} as a shared library" OFF)

# WinPipe Library
if(BLVCKWINPIPE_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BLVCKWINPIPE_EXPORTS BLVCKWINPIPE_DLL)
else()
    add_library(${PROJECT_NAME} STATIC)
endif()

add_subdirectory(src)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${LIB_OUT_DIR}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${LIB_OUT_DIR}/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY "${BIN_OUT_DIR}/$<CONFIG>"
)