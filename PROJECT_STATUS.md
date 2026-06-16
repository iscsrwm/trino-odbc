# Project Status: Trino ODBC Driver

**Assessment date:** 2026-06-16
**Verdict:** Not production quality — early-stage scaffold / proof-of-concept.

The codebase is well-organized at the surface level (clean module layout,
consistent style, mutex usage, `-Wall -Wextra -Wpedantic`), but it has
disqualifying defects in its core data path: it cannot actually return query
results, and it does not interoperate with a real ODBC Driver Manager.

> Note: despite "C++" framing, this is a pure **C** project (`LANGUAGES C`,
> all sources are `.c`).

---

## Summary scorecard

| Aspect                     | Verdict                                   |
|----------------------------|-------------------------------------------|
| Code organization & style  | Good                                      |
| Core functionality         | **Broken — returns no data**              |
| ODBC spec compliance       | **Fails — incompatible return codes/ABI** |
| Memory safety              | Known leaks (pagination path)             |
| Tests                      | Don't cover the critical path             |
| CI / tooling / hygiene     | Missing                                   |

---

## Critical / blocking issues

### 1. The driver never parses or returns any row data
- `src/protocol/client.c:366-369` — column and row parsing are unimplemented TODOs.
- `results->columns`, `results->rows`, `results->row_count`, `results->column_count`
  remain 0/NULL.
- `SQLFetch` (`src/resultset/resultset.c:62`) sees `row_count == 0` and returns
  `SQL_NO_DATA`. Every query appears to return zero rows.

### 2. Two competing JSON parsers; the good one is never wired in
- `src/protocol/response.c` has a proper `json-c`-based parser
  (`trino_parse_columns_jsonc`), but the live request path in `client.c` uses a
  hand-rolled `strstr`-based scanner that cannot handle nested arrays — and it
  doesn't parse rows at all. The robust code is effectively dead.

### 3. Pagination is broken and leaks
- `src/protocol/client.c:399` — `TODO: Free previous row data` (leak).
- `src/protocol/client.c:470` — `TODO: Parse new rows from data array`.
- Multi-page Trino result sets (the norm) do not work.

### 4. New HTTP client created/destroyed per statement execution
- `src/statement/statement.c:209,274` — a `trino_http_client_t` is created and
  destroyed on every execute; `src/connection/connection_pool.c` is a no-op stub.
  No TCP/TLS connection reuse, contradicting the README's "connection pooling".

### 5. Defines its own ODBC ABI instead of using system `sql.h`/`sqlext.h`
- `include/trino_odbc.h:16-22` redefines return codes with colliding/incorrect
  values:
  - `SQL_ERROR == 100` and `SQL_NO_DATA == 100` (identical!).
  - Real ODBC: `SQL_SUCCESS=0`, `SQL_SUCCESS_WITH_INFO=1`, `SQL_NO_DATA=100`,
    `SQL_ERROR=-1`, `SQL_INVALID_HANDLE=-2`, `SQL_NEED_DATA=99`.
- A real Driver Manager (unixODBC/iODBC) will misinterpret every return code.
  The driver cannot interoperate with an actual ODBC Driver Manager.

### 6. Bound parameters are never substituted
- `substitute_parameters()` (`src/statement/prepared.c:158`) is never called;
  `SQLExecute`/`SQLExecDirect` send raw SQL containing `?` placeholders.
- `format_parameter_value` is always invoked with `SQL_C_CHAR`
  (`src/statement/prepared.c:181`), so all params would be emitted as quoted
  strings regardless of bound type.

---

## Significant issues

- **`SQLGetData` type handling** (`src/resultset/resultset.c:299-310`):
  `SQL_C_BINARY` collides with `SQL_C_SHORT` (a symptom of the custom ABI);
  numeric conversions use unchecked `atoi`/`atof`/`atoll` (no overflow/range checks).
- **Naive write-op detection** (`src/statement/statement.c:182`): case-sensitive
  `strncmp` for uppercase keywords only; misses lowercase SQL, leading comments,
  and CTEs (`WITH ... INSERT`).
- **HTTP header injection risk** (`src/protocol/client.c:256-268`): user-controlled
  `source`/`client_tags` interpolated into headers via `snprintf` with no validation.
- **`SQLPrepare` doesn't parse parameters**; prepared statements with bound params
  won't substitute correctly given the protocol layer is incomplete.

---

## Missing connection entry points

- **No `SQLConnect` / `SQLDriverConnect` / `SQLDisconnect`** are implemented or
  exported. A real ODBC Driver Manager connects exclusively through these, so
  the driver cannot be opened by any DM-based application — even though the
  internal connection logic (`trino_conn_connect`, `trino_parse_conn_string`)
  exists. Tracked as P0.5 in `REMEDIATION_PLAN.md`.

## Project hygiene gaps

- **No CI/CD** (no `.github/workflows`).
- **No `.clang-format` / `.clang-tidy`**, no static analysis (though
  `compile_commands.json` is exported).
- **Stray artifacts:** `include/trino_odbc/resultset.h.orig` (merge leftover),
  duplicate `build_windows.md` / `BUILD_WINDOWS.md`, duplicate
  `windows/odbc.ini` / `windows/odbcinst.ini`, and 3 orphaned integration-test
  stubs not referenced by CMake.
- **Misleading tests:** integration tests are mostly stubs that print "skipped";
  unit tests exercise helpers but never the end-to-end query path — which is why
  the broken result path was not caught.
- **README over-claims:** advertises connection pooling (stub), Kerberos auth
  (no implementation), and "production-quality" — none accurate.
- **MinGW shared/static mismatch:** `cmake/MingwW64Toolchain.cmake` sets
  `BUILD_SHARED_LIBS OFF` but `src/CMakeLists.txt` hardcodes
  `add_library(... SHARED ...)`.
- **Hardcoded paths:** `create_windows_package.sh` uses `/root/...` paths.

---

## Bottom line

This is best described as an early-stage scaffold or proof-of-concept, not a
production driver. The structure is a reasonable foundation, but to be
production-quality it needs, at minimum:

1. Real column/row parsing wired into the live path.
2. Working pagination.
3. Use of the real ODBC headers with correct return-code values.
4. Connection reuse.
5. End-to-end tests against a real/mocked Trino.
6. CI plus static analysis/sanitizers.

See `REMEDIATION_PLAN.md` for the prioritized path to production quality.
