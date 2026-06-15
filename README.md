# Trino ODBC Driver

Production-quality ODBC 3.x driver for the Trino distributed SQL query engine.

## Features

- Full ODBC 3.x compliance (core + extended features)
- Support for all Trino authentication methods (NONE, PASSWORD, CERTIFICATE, KERBEROS)
- Connection pooling
- Query cancellation
- Result set caching for cursor support
- Comprehensive error handling and diagnostics
- Thread-safe handle management
- Cross-platform (Linux, macOS, Windows)

## Building

### Prerequisites

- CMake 3.16+
- GCC 7+ or Clang 6+
- libcurl development headers
- unixODBC development headers (for testing)

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Testing

```bash
ctest --output-on-failure
```

### Installation

```bash
sudo make install
```

## Connection String

```
Driver={Trino ODBC};Server=localhost;Port=8080;User=admin;Catalog=memory;Schema=default;Authentication=NONE
```

### Connection Properties

| Property | Description | Default |
|----------|-------------|---------|
| Server | Trino coordinator host | localhost |
| Port | Trino coordinator port | 8080 |
| User | Username | (empty) |
| Password | Password | (empty) |
| Catalog | Default catalog | memory |
| Schema | Default schema | default |
| Authentication | Auth method | NONE |
| SSL | Enable TLS | false |
| SSLTrustStoreCertificate | CA certificate path | (system) |
| QueryTimeout | Query timeout in seconds | 300 |
| Source | Client identifier | trino-odbc |

### Authentication Methods

- `NONE` — No authentication
- `PASSWORD` — Basic authentication (username/password)
- `CERTIFICATE` — Mutual TLS with client certificate
- `KERBEROS` — Kerberos/SPNEGO authentication

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

- Trino is read-only: INSERT, UPDATE, DELETE operations return SQL_ERROR
- Native server-side cursors are not supported; scrollable cursors use client-side caching
- Transaction control is limited (Trino does not support multi-statement transactions)
- Complex types (array, map, row) are returned as JSON strings

## License

MIT
