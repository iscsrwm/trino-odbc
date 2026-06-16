# Remediation Plan: Trino ODBC Driver → Production Quality

Ordered by priority. Each item lists the problem, the fix, the files involved,
and rough effort. See `PROJECT_STATUS.md` for the full assessment.

---

## P0 — Blockers (driver is non-functional / non-compliant without these)

### P0.1 — Fix the ODBC return-code ABI
- **Problem:** `include/trino_odbc.h:16-22` defines a custom ABI where
  `SQL_ERROR == SQL_NO_DATA == 100` and `SQL_ERROR` is positive. Real ODBC
  requires `SQL_SUCCESS=0`, `SQL_SUCCESS_WITH_INFO=1`, `SQL_NO_DATA=100`,
  `SQL_ERROR=-1`, `SQL_INVALID_HANDLE=-2`, `SQL_NEED_DATA=99`. No real Driver
  Manager will work with the current values.
- **Fix:** Preferably include the system `<sql.h>`/`<sqlext.h>`/`<sqltypes.h>`
  from unixODBC and delete the hand-rolled typedefs/macros. At minimum, correct
  every value to match the spec. System headers also give correct `SQLCHAR`,
  `SQLWCHAR`, `SQL_C_*`, and documented signatures.
- **Files:** `include/trino_odbc.h` (+ ripple across all `.c` files).
- **Effort:** Medium–High (mechanical but wide). **Do this first.**

### P0.2 — Implement column + row parsing in the live query path
- **Problem:** `src/protocol/client.c:366-369` leaves column/row parsing as TODOs,
  so every query returns 0 columns / 0 rows; `SQLFetch` always returns `SQL_NO_DATA`.
- **Fix:** Replace the hand-rolled `strstr` JSON scanner in `client.c` with
  `json-c` (already a dependency, already used in `response.c`). Parse the full
  `QueryResults` object: `id`, `nextUri`, `columns[]`, `data[][]`, `stats`,
  `error`. Populate `results->columns`, `results->rows` (`SQLCHAR***`),
  `results->row_count`, `results->column_count`. Reuse/extend
  `trino_parse_columns_jsonc`.
- **Files:** `src/protocol/client.c`, `src/protocol/response.c`.
- **Effort:** High. This is the core of the driver.

### P0.3 — Implement pagination (fetch_next) with proper row accumulation
- **Problem:** `src/protocol/client.c:399,470` — `fetch_next` doesn't parse new
  rows and leaks the previous page. Trino streams results across many `nextUri`
  pages, so single-page-only is unusable.
- **Fix:** In `fetch_next`, free the prior page's rows, parse the new `data[][]`,
  append/replace rows, update `nextUri`/`state`. Wire `SQLFetch`
  (`src/resultset/resultset.c:62-69`) to call `trino_http_client_fetch_next` when
  local rows are exhausted and `next_uri != NULL` instead of returning `SQL_NO_DATA`.
- **Files:** `src/protocol/client.c`, `src/resultset/resultset.c`.
- **Effort:** Medium–High.

### P0.4 — Actually substitute bound parameters before executing
- **Problem:** `substitute_parameters()` (`src/statement/prepared.c:158`) is never
  called; raw SQL with `?` placeholders is sent. `format_parameter_value` is always
  called with `SQL_C_CHAR` (`src/statement/prepared.c:181`), so all params are
  emitted as quoted strings regardless of bound type.
- **Fix:** Call `substitute_parameters` in `trino_stmt_exec_direct` before sending;
  pass the record's real `c_type` to `format_parameter_value`; handle NULL via
  `str_len_or_ind == SQL_NULL_DATA`. Ideally migrate to Trino server-side prepared
  statements (`X-Trino-Prepared-Statement` / `EXECUTE`) to avoid SQL injection.
- **Files:** `src/statement/statement.c`, `src/statement/prepared.c`.
- **Effort:** Medium (substitution) / High (server-side prepare).

---

### P0.5 — Implement SQLConnect / SQLDriverConnect / SQLDisconnect
- **Problem:** The driver implements no connection entry points — there is no
  `SQLConnect`, `SQLDriverConnect`, or `SQLDisconnect` exported. A real ODBC
  Driver Manager (unixODBC/iODBC) connects exclusively through these, so the
  driver cannot be opened by any DM-based application despite the connection
  parsing/lifecycle logic existing internally (`trino_conn_connect`,
  `trino_parse_conn_string`). (Discovered while building the end-to-end tests,
  which had to call `trino_conn_connect` directly.)
- **Fix:** Add the standard entry points:
  - `SQLConnect(dbc, dsn, ..., user, ..., auth, ...)` — look up the DSN (via the
    DM/odbc.ini) or treat it as a server, build a `trino_conn_config_t`, call
    `trino_conn_connect`.
  - `SQLDriverConnect(dbc, hwnd, inConnStr, ..., outConnStr, ..., completion)` —
    parse the full connection string with `trino_parse_conn_string`, connect,
    and write back the completed connection string.
  - `SQLDisconnect(dbc)` — wrap `trino_conn_disconnect`.
  - Wire these into `SQLGetFunctions` (already advertises `SQLConnect`/
    `SQLDisconnect`/`SQLDriverConnect`).
- **Files:** new `src/connection/connect.c` (or extend `connection.c`),
  `src/connection/info.c` (SQLGetFunctions already lists them).
- **Effort:** Medium. Required for any real DM usage.

## P1 — Correctness & safety (needed before trusting in production)

### P1.1 — Reuse one HTTP client per connection
- **Problem:** `src/statement/statement.c:209-274` creates/destroys an HTTP client
  per execute; `connection_pool.c` is a no-op stub. No TCP/TLS reuse.
- **Fix:** Store a persistent `trino_http_client_t` on the connection and reuse the
  curl easy/share handle. Don't destroy it after each execute. Decide whether
  "pooling" is in scope or remove the README claim.
- **Files:** `src/statement/statement.c`, `src/connection/connection.c`,
  `src/connection/connection_pool.c`.

### P1.2 — Fix `SQLGetData` type handling — DONE
- **Problem:** `SQL_C_BINARY` collided with `SQL_C_SHORT` (a symptom of the custom
  ABI) and numeric conversions used unchecked `atoi/atof/atoll`.
- **Resolved:** The custom ABI collision was eliminated by P0.1. `SQLGetData` now
  handles char/wchar (UTF-8 bytes), binary, bit, all signed/unsigned integer
  widths (with range checks via `strtoll`/`strtoull`), float/double (`strtod`),
  and date/time/timestamp into the ODBC C structs. Invalid/out-of-range values
  return `SQL_ERROR`; character/binary truncation returns `SQL_SUCCESS_WITH_INFO`
  and sets SQLSTATE `01004`. Covered by e2e tests.
- **Files:** `src/resultset/resultset.c`.
- **Remaining:** full `SQL_C_WCHAR` (UTF-16) conversion and `SQL_C_NUMERIC`
  struct output are still approximate (wchar treated as UTF-8 bytes; numeric
  falls back to string).

### P1.3 — Harden write-op detection
- **Problem:** `src/statement/statement.c:182` uses case-sensitive `strncmp`;
  misclassifies lowercase SQL, leading comments, and CTEs (`WITH ... INSERT`).
- **Fix:** Case-insensitive comparison, skip leading comments/whitespace, tokenize
  the first keyword. Consider relying on Trino's response shape (`updateType`).
- **Files:** `src/statement/statement.c`.

### P1.4 — Validate/escape HTTP header inputs
- **Problem:** `src/protocol/client.c:256-268` interpolates user-controlled
  `source`/`client_tags` into headers with no validation (injection/truncation).
- **Fix:** Reject/escape CR/LF, enforce length limits, prefer `curl_easy_escape`
  where appropriate.
- **Files:** `src/protocol/client.c`.

### P1.5 — Audit memory ownership and run under sanitizers
- **Problem:** Many manual alloc/free paths; `ENABLE_SANITIZERS` exists but isn't
  exercised.
- **Fix:** Build with `-DENABLE_SANITIZERS=ON`, run unit + (new) integration tests
  under ASan/UBSan and Valgrind. Verify `trino_query_results_free` matches what the
  new parser allocates (rows are `SQLCHAR***`).
- **Files:** all of `src/`, plus CI.

---

## P2 — Test coverage (so regressions are caught)

### P2.1 — End-to-end query tests
- **Problem:** Integration tests `test_connect.c`/`test_query.c`/`test_resultset.c`
  are stubs that print "skipped" and aren't referenced in `test/CMakeLists.txt`.
  The broken result path slipped through because nothing tests it.
- **Fix:** Add a mock HTTP layer (or libcurl stub) feeding canned Trino
  `QueryResults` JSON; assert `SQLExecDirect → SQLFetch → SQLGetData` returns the
  expected rows/columns/types. Add a multi-page (`nextUri`) case and an
  error-response case.
- **Files:** `test/integration/*`, `test/CMakeLists.txt`.

### P2.2 — Optional live integration tests
- A CTest label (already partly set up as `"integration"`) gated behind an env var
  pointing at a real Trino, run in CI via a Trino container.

---

## P3 — Tooling, CI, and hygiene

- **CI:** Add `.github/workflows/ci.yml` — build on Linux (+ MinGW cross), run
  `ctest`, run ASan/UBSan + Valgrind, run `clang-tidy`.
- **Static analysis / formatting:** Add `.clang-format` and `.clang-tidy`; enforce
  in CI. `compile_commands.json` is already exported.
- **Fix MinGW shared/static mismatch:** toolchain sets `BUILD_SHARED_LIBS OFF` but
  `src/CMakeLists.txt` hardcodes `add_library(... SHARED ...)`. Respect
  `BUILD_SHARED_LIBS`.
- **Remove stray artifacts:** `include/trino_odbc/resultset.h.orig`, duplicate
  `build_windows.md`/`BUILD_WINDOWS.md`, duplicate `windows/odbc.ini`/`odbcinst.ini`,
  orphaned integration stubs. De-hardcode `/root/...` paths in
  `create_windows_package.sh`.
- **README accuracy:** Remove or implement "connection pooling" and "Kerberos auth";
  drop "production-quality" until P0/P1 are done. Add a known-limitations section
  that matches reality.

---

## Suggested sequencing

1. **P0.1** (ABI) — unblocks everything and likely auto-fixes P1.2's type collision.
2. **P0.2 + P0.3** (parse columns/rows + pagination) — makes the driver return data.
3. **P2.1** (e2e tests) — lock in the fix immediately.
4. **P0.4 + P1.1** (params + client reuse).
5. **P1.2–P1.5** (correctness/safety hardening under sanitizers).
6. **P3** (CI/tooling/hygiene) — stand up CI early (alongside step 3).

**Realistic effort to reach a genuine 1.0:** several focused weeks, with
P0.1–P0.3 being the bulk of the work and the highest risk.
