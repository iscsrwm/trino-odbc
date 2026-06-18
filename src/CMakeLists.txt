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
    ${CMAKE_SOURCE_DIR}/src/error/diag.c
    ${CMAKE_SOURCE_DIR}/src/error/log.c
)

set(CATALOG_SOURCES
    ${CMAKE_SOURCE_DIR}/src/catalog/catalog.c
)

set(TRANSACTION_SOURCES
    ${CMAKE_SOURCE_DIR}/src/transaction/transaction.c
)

set(UNICODE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/unicode/unicode_stmt.c
)

# Windows-only setup GUI (ConfigDSN dialog shown by the ODBC Administrator).
# The .c is guarded by #ifdef _WIN32 and the .rc is only compiled on Windows.
if(WIN32)
    set(SETUP_SOURCES
        ${CMAKE_SOURCE_DIR}/src/setup/setup.c
        ${CMAKE_SOURCE_DIR}/src/setup/setup.rc
    )
else()
    set(SETUP_SOURCES)
endif()

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
    ${UNICODE_SOURCES}
    ${SETUP_SOURCES}
)

# The setup dialog resource needs to find resource.h via an include dir.
if(WIN32)
    target_include_directories(trino_odbc PRIVATE ${CMAKE_SOURCE_DIR}/src/setup)
endif()

# Link dependencies
target_link_libraries(trino_odbc PRIVATE CURL::libcurl Threads::Threads ${JSON_C_TARGET})

# On Windows, an ODBC driver links the ODBC import libraries. odbccp32 (the ODBC
# installer library, used by the setup GUI) internally calls StringCchPrintfW,
# which references the legacy stdio symbol _vsnwprintf_s. With the static CRT
# (/MT) that symbol lives in legacy_stdio_definitions.lib, so link it to resolve
# "unresolved external symbol _vsnwprintf_s".
# user32/comdlg32 provide the dialog APIs used by the setup GUI.
if(WIN32)
    target_link_libraries(trino_odbc PRIVATE
        odbc32 odbccp32 legacy_stdio_definitions user32 comdlg32)
endif()

# Set library properties
set_target_properties(trino_odbc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME trino_odbc
)

# On Windows, use an explicit .def file for controlled exports
if(WIN32)
    set_target_properties(trino_odbc PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/trino_odbc.def"
    )
endif()

# Compiler warnings. MSVC uses a different flag set than GCC/Clang.
if(MSVC)
    target_compile_options(trino_odbc PRIVATE /W3)
    # Our code uses standard C string functions and POSIX names (strdup); silence
    # MSVC's deprecation warnings for both.
    target_compile_definitions(trino_odbc PRIVATE
        _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
    target_compile_options(trino_odbc PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Public headers
target_include_directories(trino_odbc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
