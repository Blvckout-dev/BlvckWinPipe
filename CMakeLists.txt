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

# Examples
add_subdirectory(examples)

# Install rules
include(GNUInstallDirs)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Config.cmake
    NAMESPACE Blvckout::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)