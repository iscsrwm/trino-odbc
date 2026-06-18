# Trino ODBC Driver

ODBC 3.x driver for the Trino distributed SQL query engine.

> **Status: working.** The driver connects, executes queries, and returns
> paginated result sets through a real ODBC Driver Manager. It has been verified
> end-to-end on **Windows** (Microsoft ODBC Driver Manager) from **.NET**
> (`System.Data.Odbc`), including multi-column/multi-row result sets and the
> full set of supported data types, and on **Linux** (unixODBC) under
> AddressSanitizer/UBSan. A native **Windows setup GUI** (DSN configuration via
> the ODBC Data Source Administrator) is included.

## Features

- ODBC 3.x entry points: connect (`SQLConnect`/`SQLDriverConnect`), execute,
  fetch, `SQLDescribeCol`, `SQLColAttribute`, `SQLGetData`, catalog functions,
  diagnostics — with both ANSI and Unicode (W) variants
- Windows setup GUI: configure a DSN from the ODBC Data Source Administrator
  (`odbcad32.exe`), including a **Test Connection** button
- Authentication: `NONE`, `PASSWORD`, `CERTIFICATE`, and `KERBEROS`/SPNEGO
  (SPNEGO requires a libcurl built with GSS/SPNEGO support)
- Streaming result sets with `nextUri` pagination
- Bound input parameters (`SQLBindParameter`)
- HTTP client reuse per connection, plus a process-wide connection pool
  (shared TCP/TLS connections, DNS, and TLS sessions across connections)
- Query cancellation
- Error handling and diagnostics
- Thread-safe handle management

## Platforms

- **Windows (x64)** — the primary distribution target. Install via the MSI
  (see [`installer/README.md`](installer/README.md)); the driver is a single
  self-contained DLL with no external runtime dependencies. Verified with the
  Microsoft ODBC Driver Manager and `System.Data.Odbc`.
- **Linux** — build with CMake against unixODBC + libcurl + json-c.

## Building

### Windows (recommended: MSI)

The Windows build uses CMake presets + vcpkg and packages an MSI. See
[`installer/README.md`](installer/README.md) for the full prerequisites and
one-shot build. In short, from an *x64 Native Tools / Developer PowerShell*:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
pwsh installer/build_msi.ps1   # produces trino_odbc-x64.msi
```

To build just the driver DLL (no MSI):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
# DLL: build-windows\src\trino_odbc.dll
```

### Linux

Prerequisites:

- CMake 3.16+
- GCC 7+ or Clang 6+
- libcurl development headers
- json-c development headers
- unixODBC development headers (for testing)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Testing

```bash
ctest --output-on-failure
```

Live integration tests against a real Trino server are gated on the
`TRINO_TEST_SERVER` environment variable (and labelled `live`); they are skipped
when it is unset, so the default `ctest` run stays green.

### Installation (Linux)

```bash
sudo make install
```

## Connecting

### By connection string (DSN-less)

```
Driver={Trino ODBC Driver};Server=trino.example.com;Port=443;Catalog=tpch;Schema=tiny;User=alice;Password=secret;Authentication=PASSWORD;SSL=true
```

### By DSN

Create a DSN with the Windows setup GUI (ODBC Data Source Administrator → Add →
*Trino ODBC Driver*), then connect with just the DSN name:

```
DSN=MyTrinoDSN
```

Connection-string keywords take precedence over stored DSN values, so you can
override a saved setting inline (e.g. `DSN=MyTrinoDSN;Password=secret` when the
DSN was saved without a password).

### Connection Properties

| Property | Aliases | Description | Default |
|----------|---------|-------------|---------|
| Server | Host | Trino coordinator host | localhost |
| Port | | Trino coordinator port | 8080 |
| User | Username | Username | (empty) |
| Password | | Password | (empty) |
| Catalog | | Default catalog | (empty) |
| Schema | | Default schema | (empty) |
| Authentication | AuthType | Auth method (`NONE`/`PASSWORD`/`CERTIFICATE`/`KERBEROS`) | NONE |
| SSL | | Enable TLS | false |
| SSLVerify | | Verify the server certificate (set `false` for self-signed/internal CAs) | true |
| SSLNoRevoke | | Skip the certificate revocation check (set `true` to work around `CRYPT_E_REVOCATION_OFFLINE` when the CRL/OCSP server is unreachable) | false |
| SSLTrustStoreCertificate | | CA certificate path | (system) |
| Source | | Client identifier sent to Trino | trino-odbc |
| ClientTags | | Trino client tags | (empty) |
| SessionProperties | | Trino session properties | (empty) |
| QueryTimeout | | Query timeout in seconds | 300 |
| ConnectTimeout | | Connect timeout in seconds | 30 |

### Authentication Methods

- `NONE` — No authentication
- `PASSWORD` — Basic authentication (username/password); aliases `BASIC`, `LDAP`
- `CERTIFICATE` — Mutual TLS with client certificate; alias `CLIENT-CERT`
- `KERBEROS` — Kerberos/SPNEGO authentication; alias `SPNEGO`

## Diagnostics / logging

Set the `TRINO_ODBC_LOG` environment variable to a writable file path to capture
a trace of ODBC calls (connection, statement, fetch) for troubleshooting. When
the variable is unset, logging is a no-op. The password is redacted in logged
connection strings.

```powershell
$env:TRINO_ODBC_LOG = "C:\Temp\trino_odbc.log"
```

## Supported Data Types

| Trino Type | ODBC SQL Type | C Type |
|------------|--------------|--------|
| varchar | SQL_VARCHAR | SQL_C_CHAR |
| char | SQL_CHAR | SQL_C_CHAR |
| varbinary | SQL_VARBINARY | SQL_C_BINARY |
| boolean | SQL_BIT | SQL_C_BIT |
| tinyint | SQL_TINYINT | SQL_C_TINYINT |
| smallint | SQL_SMALLINT | SQL_C_SHORT |
| integer | SQL_INTEGER | SQL_C_LONG |
| bigint | SQL_BIGINT | SQL_C_BIGINT |
| real | SQL_REAL | SQL_C_FLOAT |
| double | SQL_DOUBLE | SQL_C_DOUBLE |
| decimal | SQL_DECIMAL | SQL_C_CHAR |
| date | SQL_TYPE_DATE | SQL_C_TYPE_DATE |
| time | SQL_TYPE_TIME | SQL_C_TYPE_TIME |
| timestamp | SQL_TYPE_TIMESTAMP | SQL_C_TYPE_TIMESTAMP |
| json | SQL_VARCHAR | SQL_C_CHAR |
| array | SQL_VARCHAR | SQL_C_CHAR |
| map | SQL_VARCHAR | SQL_C_CHAR |
| row | SQL_VARCHAR | SQL_C_CHAR |

## Known Limitations

- Native server-side cursors are not supported; scrollable cursors use client-side caching
- Transaction control is limited (Trino does not support multi-statement transactions)
- Complex types (array, map, row) are returned as JSON strings
- The setup GUI and DSN resolution are Windows-only; on Linux, configure DSNs via
  unixODBC's `odbc.ini`/`odbcinst.ini`

## Documentation

- [`installer/README.md`](installer/README.md) — building and shipping the Windows MSI
- [`BUILD_WINDOWS.md`](BUILD_WINDOWS.md) — Windows build details and manual registration
- [`windows/README.md`](windows/README.md) — manual (non-MSI) Windows install

## License

MIT
