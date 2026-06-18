# Project Status: Trino ODBC Driver

**Last updated:** 2026-06-18
**Verdict:** Working — connects, executes, and returns result sets through a
real ODBC Driver Manager, with a Windows setup GUI.

> This is a pure **C** project (`LANGUAGES C`, all sources are `.c`).

This document supersedes the earlier (2026-06-16) "early-stage scaffold"
assessment. The blocking defects identified then have since been fixed; the
items below reflect the current state. See `REMEDIATION_PLAN.md` for the history
of what was done and the remaining hardening backlog.

---

## Summary scorecard

| Aspect                     | Verdict                                                        |
|----------------------------|----------------------------------------------------------------|
| Code organization & style  | Good                                                           |
| Core functionality         | Working — connect, execute, fetch, paginated result sets       |
| ODBC DM interop            | Working — verified with the Microsoft ODBC DM + .NET, and unixODBC |
| Windows setup GUI          | Working — `ConfigDSN` dialog with Test Connection              |
| Data types                 | Working — integer/bigint/double/varchar/boolean/NULL verified  |
| Memory safety              | Clean under ASan/UBSan on the covered paths                    |
| Tests                      | Unit + mock-transport e2e + optional live integration          |

---

## What works

- **Connection entry points:** `SQLConnect`, `SQLDriverConnect`(+`W`),
  `SQLDisconnect`, with DSN resolution from `ODBC.INI` on Windows. Precedence is
  defaults → stored DSN keywords → connection-string keywords.
- **Query path:** `SQLExecDirect`(+`W`), `SQLPrepare`(+`W`), `SQLExecute`,
  `SQLNumResultCols`, `SQLDescribeCol`(+`W`), `SQLColAttribute`(+`W`),
  `SQLFetch`, `SQLGetData`, `SQLRowCount`, `SQLMoreResults`.
- **Result sets:** column/row parsing via json-c, `nextUri` pagination,
  client-side row buffering.
- **Data types:** verified end-to-end through .NET — `INTEGER`, `BIGINT`,
  `DOUBLE`, `VARCHAR`, `BOOLEAN`, and `NULL`; plus the full mapping table in the
  README (char/binary/decimal/date/time/timestamp, complex types as JSON text).
- **Descriptors:** the four automatically-allocated descriptors (APD/IPD/ARD/IRD)
  are created per statement and returned from `SQLGetStmtAttr`, which the Windows
  DM requires.
- **Unicode:** the driver advertises and exports the W-suffixed entry points the
  Windows DM calls in Unicode mode (`SQLDriverConnectW`, `SQLExecDirectW`,
  `SQLDescribeColW`, `SQLColAttributeW`, `SQLGet/SetStmtAttrW`,
  `SQLGet/SetConnectAttrW`, `SQLGet/SetEnvAttrW`, catalog W functions, …).
- **`SQLGetFunctions`:** implements the `SQL_API_ODBC3_ALL_FUNCTIONS` bitmap so
  the DM knows which functions are supported.
- **Windows setup GUI:** `ConfigDSN`/`ConfigDSNW` show a native dialog (core +
  advanced fields) with a **Test Connection** button that dials the server
  through the real driver manager. Built into the driver DLL.
- **Connection reuse:** one HTTP client per connection plus a process-wide
  curl share handle (pooled TCP/TLS/DNS/TLS-session).
- **Packaging:** a self-contained MSI (curl/json-c/CRT statically linked) that
  registers the driver and the setup DLL.

## Verified

- **Windows + .NET (`System.Data.Odbc`):** `SELECT 1` → `1`; multi-column /
  multi-row reads (`ExecuteReader`); `SELECT COUNT(*)` over a real table;
  mixed-type projection with NULL handling; connecting via a GUI-created
  `DSN=...`.
- **Linux + unixODBC:** unit and mock-transport end-to-end tests run clean under
  AddressSanitizer/UBSan. An optional live test runs against a real Trino
  (`TRINO_TEST_SERVER`).

---

## Remaining hardening backlog

These are quality/robustness items, not blockers (see `REMEDIATION_PLAN.md`):

- Harden write-op detection (case/comments/CTEs) — P1.3.
- Validate/escape user-controlled HTTP header inputs (`source`/`client_tags`) — P1.4.
- Broaden sanitizer/Valgrind coverage across all paths — P1.5.
- Expand automated test coverage (more types, multi-page, error responses) — P2.
- CI/static-analysis/formatting hygiene — P3.

## Known limitations

- No native server-side cursors; scrollable cursors use client-side caching.
- Limited transaction control (Trino has no multi-statement transactions).
- Complex types (`array`/`map`/`row`) are surfaced as JSON strings.
- The setup GUI and DSN auto-resolution are Windows-only.
