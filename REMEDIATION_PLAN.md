# Remediation Plan: Trino ODBC Driver → Production Quality

Ordered by priority. Each item lists the problem, the fix, the files involved,
and rough effort. See `PROJECT_STATUS.md` for the current assessment.

> **Status (2026-06-18):** All P0 blockers are **DONE** — the driver connects,
> executes, and returns result sets through a real ODBC Driver Manager, verified
> on Windows from .NET and on Linux under sanitizers. P1.1 and P1.2 are also
> done. Remaining work (P1.3–P1.5, P2, P3) is hardening, test coverage, and
> tooling. A Windows setup GUI (`ConfigDSN`) and DSN resolution were added on top
> of the original plan (see "Post-plan work" at the end).

---

## P0 — Blockers (driver is non-functional / non-compliant without these) — ALL DONE

### P0.1 — Fix the ODBC return-code ABI — DONE
- **Resolved:** the driver now uses the system `<sql.h>`/`<sqlext.h>`/
  `<sqltypes.h>` headers (via `include/trino_odbc.h`); the hand-rolled,
  colliding return-code macros are gone. Return codes match the spec, so a real
  DM interprets them correctly.
- **Resolved:** system ODBC headers are used; spec-correct return codes,
  `SQLCHAR`/`SQLWCHAR`/`SQL_C_*` types, and documented signatures throughout.
- **Files:** `include/trino_odbc.h` and all `.c` files.

### P0.2 — Implement column + row parsing in the live query path — DONE
- **Resolved:** `src/protocol/response.c` parses the full `QueryResults` object
  with json-c (`id`, `nextUri`, `columns[]`, `data[][]`, `stats`, `error`) and
  populates the result set. `SQLFetch`/`SQLGetData` return real rows.
- **Files:** `src/protocol/client.c`, `src/protocol/response.c`.

### P0.3 — Implement pagination (fetch_next) with proper row accumulation — DONE
- **Resolved:** `trino_http_client_query` follows the `nextUri` chain until
  columns and a data page are available; `fetch_next` parses subsequent pages and
  frees the prior page. Verified against real Trino result sets.
- **Files:** `src/protocol/client.c`, `src/resultset/resultset.c`.

### P0.4 — Actually substitute bound parameters before executing — DONE
- **Resolved:** bound parameters are substituted before sending, using each
  record's bound C type, with NULL handling. Verified via the bound-parameter
  e2e/live tests.
- **Files:** `src/statement/statement.c`, `src/statement/prepared.c`.

---

### P0.5 — Implement SQLConnect / SQLDriverConnect / SQLDisconnect — DONE
- **Resolved:** `SQLConnect`, `SQLDriverConnect`(+`W`), and `SQLDisconnect` are
  implemented in `src/connection/connect.c` and exported. `SQLConnect` resolves a
  real DSN's keywords from `ODBC.INI` (falling back to host[:port]);
  `SQLDriverConnect` resolves `DSN=` then overlays the connection string. These
  are advertised in `SQLGetFunctions`.
- **Files:** `src/connection/connect.c`, `src/connection/connection.c`,
  `src/connection/info.c`.

## P1 — Correctness & safety (needed before trusting in production)

### P1.1 — Reuse one HTTP client per connection — DONE
- **Problem:** the HTTP client was created/destroyed per execute; the easy/share
  handle and TCP/TLS connection were never reused; `connection_pool.c` was a
  no-op stub.
- **Resolved:** each connection caches and reuses a single `trino_http_client_t`
  (created lazily, owned by the connection). Additionally, `connection_pool.c`
  now provides a reference-counted, process-wide curl share handle
  (`trino_http_pool_acquire`/`release`) that pools TCP/TLS connections, DNS, and
  TLS sessions across all connections; every easy handle attaches to it via
  `CURLOPT_SHARE`. Thread-safe via per-data lock callbacks.
- **Files:** `src/connection/connection.c`, `src/connection/connection_pool.c`,
  `src/protocol/client.c`.

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
- **Update:** the `SQLGetData` conversion matrix is now complete — char, wchar
  (UTF-8 -> UTF-16), binary, all signed/unsigned integer widths, float/double,
  bit, date/time/timestamp, and `SQL_C_NUMERIC` (decimal string parsed into
  sign/scale/precision + little-endian mantissa).

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

### P2.2 — Live integration tests — DONE
- `test/integration/test_live_trino.c` connects via `SQLDriverConnect` and runs
  real queries against Trino's built-in `tpch`/`system` catalogs (literal SELECT,
  multi-row fetch, bound parameter, error). It is gated on `TRINO_TEST_SERVER`
  and skips (passes) when unset, so the default `ctest` run stays green. Labelled
  `"live"` (`ctest -L live`). A `live-trino` CI job runs it against a
  `trinodb/trino` service container.
- **Caught a real bug:** exercising the actual libcurl path (which the in-process
  mock-transport tests bypass) surfaced a use-after-free — the reused easy handle
  kept pointing at the POST's freed header list on the subsequent GET. Fixed by
  always (re)setting `CURLOPT_HTTPHEADER` and detaching it before freeing.

---

## P3 — Tooling, CI, and hygiene

- **CI:** Add `.github/workflows/ci.yml` — build on Linux (+ MinGW cross), run
  `ctest`, run ASan/UBSan + Valgrind, run `clang-tidy`.
- **Static analysis / formatting:** Add `.clang-format` and `.clang-tidy`; enforce
  in CI. `compile_commands.json` is already exported.
- **Fix MinGW shared/static mismatch:** toolchain sets `BUILD_SHARED_LIBS OFF` but
  `src/CMakeLists.txt` hardcodes `add_library(... SHARED ...)`. Respect
  `BUILD_SHARED_LIBS`.
- **Remove stray artifacts:** `include/trino_odbc/resultset.h.orig`, orphaned
  integration stubs. (The hand-rolled `create_windows_package.sh` and the
  `trino-odbc-windows-x64/` package dir were removed — the MSI replaces them.)
- **README accuracy — DONE:** README/PROJECT_STATUS/BUILD_WINDOWS/installer docs
  were updated to reflect the working state (DM interop, setup GUI, MSI), correct
  connection keywords, and a realistic known-limitations section.

---

## Suggested sequencing

1. **P0.1** (ABI) — unblocks everything and likely auto-fixes P1.2's type collision.
2. **P0.2 + P0.3** (parse columns/rows + pagination) — makes the driver return data.
3. **P2.1** (e2e tests) — lock in the fix immediately.
4. **P0.4 + P1.1** (params + client reuse).
5. **P1.2–P1.5** (correctness/safety hardening under sanitizers).
6. **P3** (CI/tooling/hygiene) — stand up CI early (alongside step 3).

P0.1–P0.5, P1.1, and P1.2 are complete; the remaining backlog is P1.3–P1.5, P2,
and P3.

---

## Post-plan work (added after the original assessment)

Work done to make the driver function with the **Windows** ODBC Driver Manager
and .NET, beyond the original Linux-focused plan:

- **Windows DM interop fixes** (found via WinDbg and DM tracing):
  - Implemented the `SQL_API_ODBC3_ALL_FUNCTIONS` bitmap in `SQLGetFunctions` so
    the DM knows which functions exist (otherwise statement allocation failed).
  - Return the four automatically-allocated descriptor handles (APD/IPD/ARD/IRD)
    from `SQLGetStmtAttr`. Previously the DM stored uninitialized handles and
    crashed in `odbc32!SetStmtAttr` (access violation) — the root cause of the
    "crash on any query" behavior.
  - Implemented `SQLDescribeCol`/`SQLDescribeColW` (the DM calls the W form
    during result processing) and advertised them.
  - Handle `SQL_DESC_CONCISE_TYPE` in `SQLColAttribute` (.NET queries it to map
    columns to CLR types; returning 0 produced "Unknown SQL type - 0").
  - Advertise `SQLFreeStmt` in the function bitmap.
  - Export and provide consistent ANSI + Unicode (W) entry points with the
    correct calling convention; controlled exports via `src/trino_odbc.def`.

- **Windows setup GUI** (`src/setup/`):
  - `ConfigDSN`/`ConfigDSNW` with a native Win32 dialog (core + advanced fields)
    and a **Test Connection** button. The driver DLL doubles as the setup DLL.
  - The Test Connection path loads `odbc32.dll` and resolves ODBC entry points
    via `GetProcAddress` so it goes through the real driver manager rather than
    binding to the driver's own exports.
  - Build: enable the CMake `RC` language on Windows; link
    `legacy_stdio_definitions` (needed by `odbccp32` under the static CRT).

- **DSN resolution:** `trino_apply_dsn` (reads a DSN's keywords from `ODBC.INI`),
  `trino_merge_conn_string` (layer keywords without resetting defaults), and
  `trino_conn_str_get_dsn`. `SQLConnect`/`SQLDriverConnect` now apply
  defaults → DSN keywords → connection-string keywords.

- **Packaging/registry consistency:** the MSI (`installer/`) ships a single
  self-contained DLL and sets both `Driver` and `Setup` to it;
  `windows/install.reg` and `windows/odbcinst.ini` were reconciled
  (`DriverODBCVer=03.80`, `Setup` pointing at the driver DLL).

**Realistic effort to reach a genuine 1.0:** the core data path and Windows DM
interop are done; remaining effort is the hardening/test/tooling backlog
(P1.3–P1.5, P2, P3).
