include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
)

FetchContent_MakeAvailable(catch2)

add_executable(
    tests
    WinHandleTests.cpp
)

target_include_directories(
    tests
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    tests
    PRIVATE
    Catch2::Catch2WithMain
    Blvckout::BlvckWinPipe
)

add_test(
    NAME WinHandleTests
    COMMAND tests
)