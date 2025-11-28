set(EXAMPLE_ECHO_SERVER EchoServer)

add_executable(
    ${EXAMPLE_ECHO_SERVER}
    EchoServer.cpp
)

target_link_libraries(
    ${EXAMPLE_ECHO_SERVER} PRIVATE ${PROJECT_NAME}
)

set_target_properties(
    ${EXAMPLE_ECHO_SERVER}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_OUT_DIR}/$<CONFIG>"
)