# Core library sources
set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/handle.c
    ${CMAKE_SOURCE_DIR}/src/core/descriptor.c
    ${CMAKE_SOURCE_DIR}/src/core/environment.c
)

set(CONNECTION_SOURCES
    ${CMAKE_SOURCE_DIR}/src/connection/connection.c
    ${CMAKE_SOURCE_DIR}/src/connection/connect.c
    ${CMAKE_SOURCE_DIR}/src/connection/connection_pool.c
    ${CMAKE_SOURCE_DIR}/src/connection/info.c
)

set(STATEMENT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/statement/statement.c
    ${CMAKE_SOURCE_DIR}/src/statement/setpos.c
    ${CMAKE_SOURCE_DIR}/src/statement/prepared.c
)

set(RESULTSET_SOURCES
    ${CMAKE_SOURCE_DIR}/src/resultset/resultset.c
    ${CMAKE_SOURCE_DIR}/src/resultset/buffer.c
)

set(PROTOCOL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/protocol/client.c
    ${CMAKE_SOURCE_DIR}/src/protocol/request.c
    ${CMAKE_SOURCE_DIR}/src/protocol/response.c
    ${CMAKE_SOURCE_DIR}/src/protocol/wchar.c
)

set(AUTH_SOURCES
    ${CMAKE_SOURCE_DIR}/src/auth/auth.c
    ${CMAKE_SOURCE_DIR}/src/auth/basic_auth.c
    ${CMAKE_SOURCE_DIR}/src/auth/certificate_auth.c
)

set(ERROR_SOURCES
    ${CMAKE_SOURCE_DIR}/src/error/error.c
    ${CMAKE_SOURCE_DIR}/src/error/state.c
)

set(CATALOG_SOURCES
    ${CMAKE_SOURCE_DIR}/src/catalog/catalog.c
)

set(TRANSACTION_SOURCES
    ${CMAKE_SOURCE_DIR}/src/transaction/transaction.c
)

# Add the shared library
add_library(trino_odbc SHARED
    ${CORE_SOURCES}
    ${CONNECTION_SOURCES}
    ${STATEMENT_SOURCES}
    ${RESULTSET_SOURCES}
    ${PROTOCOL_SOURCES}
    ${AUTH_SOURCES}
    ${ERROR_SOURCES}
    ${CATALOG_SOURCES}
    ${TRANSACTION_SOURCES}
)

# Link dependencies
target_link_libraries(trino_odbc PRIVATE CURL::libcurl Threads::Threads)

# Set library properties
set_target_properties(trino_odbc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME trino_odbc
)

# Compiler warnings
target_compile_options(trino_odbc PRIVATE -Wall -Wextra -Wpedantic)

# Public headers
target_include_directories(trino_odbc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
