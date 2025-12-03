include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
)

FetchContent_MakeAvailable(catch2)

set(TEST_TARGET_NAME tests)

add_executable(
    ${TEST_TARGET_NAME}
)

add_subdirectory(Utils)

target_include_directories(
    ${TEST_TARGET_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    ${TEST_TARGET_NAME}
    PRIVATE
    Catch2::Catch2WithMain
    Blvckout::BlvckWinPipe
)

include(Catch)
catch_discover_tests(${TEST_TARGET_NAME})