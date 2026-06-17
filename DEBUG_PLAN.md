# Debug Plan: SQLSetStmtAttrW Access Violation (0xC0000005)

## Symptom
`.NET` `OdbcCommand.ExecuteScalar()` crashes with `System.AccessViolationException`
at `UnsafeNativeMethods.SQLSetStmtAttrW`. Crash occurs even when the driver's
`SQLSetStmtAttrW` is a no-op (`return SQL_SUCCESS;`), meaning the fault is in the
call path, NOT our function body.

## Confirmed Facts
- Connection, statement allocation, `SQLGetStmtAttrW` all WORK.
- A no-op `SQLSetStmtAttrW` STILL crashes (rules out our function logic).
- Calling `SQLSetStmtAttrW` via `GetProcAddress` directly WORKS (logs entry/exit).
- DLL and process are both x64.
- Export exists, same format as the working `SQLGetStmtAttrW`.
- Removing the export from the .def did NOT stop .NET from calling it.

## Key Inference
The DIRECT (`GetProcAddress`) call works but the DM-DISPATCHED call crashes.
The fault is in how `odbc32.dll` (the Driver Manager) dispatches to our function,
OR in a CRT/stack mismatch that only manifests on the DM's call path.

---

## STEP 1 — Capture the exact faulting address (do this FIRST)

Install "Debugging Tools for Windows" (part of the Windows SDK) to get `cdb.exe`.

```powershell
$cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"

# Minimal native repro via C# (avoids PowerShell's own frames)
@'
using System.Data.Odbc;
class P {
  static void Main() {
    var cs = System.Environment.GetEnvironmentVariable("TRINO_CS");
    using var c = new OdbcConnection(cs);
    c.Open();
    using var cmd = c.CreateCommand();
    cmd.CommandText = "SELECT 1";
    System.Console.WriteLine(cmd.ExecuteScalar());
  }
}
'@ | Set-Content repro.cs
& "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /r:System.Data.dll repro.cs

$env:TRINO_CS = $cs
$env:TRINO_ODBC_LOG = "C:\Temp\trino_odbc.log"

# Break on the access violation, dump native stack + faulting instruction + modules
& $cdb -c "sxe av; g; k; r; .lastevent; lmDvm trino_odbc; lmDvm odbc32; .ecxr; u @rip-10 L10; q" repro.exe
```

WHAT TO REPORT BACK:
1. The `k` (stack) at the time of the AV — which module is `@rip` in?
2. `lmDvm trino_odbc` load base, and `lmDvm odbc32` load base.
3. The faulting `@rip` value.
4. The `u @rip-10 L10` disassembly around the fault.

INTERPRETATION:
- If `@rip` is inside `odbc32.dll` -> the DM itself faults (handle-map / dispatch bug).
- If `@rip` is inside `trino_odbc.dll` -> compute `@rip - base`, compare to
  `dumpbin /EXPORTS` address of SQLSetStmtAttrW. Confirms whether it's even our fn.
- If `@rip` is a tiny/garbage value (e.g. 0x0000000000000003) -> a function POINTER
  is being called through a bad/uninitialized slot (dispatch-table corruption).

---

## STEP 2 — Rule out the static CRT (cheap, high value)

A new DIAGNOSTIC preset `windows-x64-dyncrt` links the DYNAMIC CRT (/MD) and the
`x64-windows` (dynamic) vcpkg triplet.

```powershell
cmake --preset windows-x64-dyncrt
cmake --build --preset windows-x64-dyncrt

# Copy the diagnostic DLL over the installed one (note: needs VC++ redist on box)
$dll = Get-ChildItem build-windows-dyncrt\src -Filter trino_odbc.dll -Recurse | Select -First 1
Copy-Item $dll.FullName "C:\Program Files\TrinoODBC\bin\trino_odbc.dll" -Force

# Re-run the .NET test
```

- If the crash DISAPPEARS -> static CRT was the cause. Make the dynamic-CRT build
  the shipping build (and bundle vc_redist, or use the /MD static-md triplet).
- If the crash PERSISTS -> CRT is NOT the cause; proceed to Step 3.

---

## STEP 3 — Enable the ODBC Driver Manager trace

This shows EXACTLY which calls the DM makes and the handle values it passes,
from the DM's side (independent of our logging).

```powershell
# Enable tracing in the ODBC Administrator (or via registry):
# HKLM\SOFTWARE\ODBC\ODBC.INI\ODBC : Trace=1, TraceFile=C:\Temp\sql.log
reg add "HKLM\SOFTWARE\ODBC\ODBC.INI\ODBC" /v Trace /t REG_SZ /d 1 /f
reg add "HKLM\SOFTWARE\ODBC\ODBC.INI\ODBC" /v TraceFile /t REG_SZ /d C:\Temp\sql.log /f

# Run the .NET test, then:
Get-Content C:\Temp\sql.log

# Disable when done
reg add "HKLM\SOFTWARE\ODBC\ODBC.INI\ODBC" /v Trace /t REG_SZ /d 0 /f
```

WHAT TO LOOK FOR:
- The last `SQLSetStmtAttr`/`SQLSetStmtAttrW` line before the trace stops.
- The HSTMT value the DM logs — does it match the driver handle we created?
- Whether the DM logs "DIAG" / error entries indicating it detected a bad return.

---

## STEP 4 — Compare working vs crashing function disassembly

If Step 1 says the fault is in our DLL, dump both functions and compare prologues.

```powershell
$dll = "C:\Program Files\TrinoODBC\bin\trino_odbc.dll"
dumpbin /DISASM:BYTES $dll > C:\Temp\disasm.txt
# Open disasm.txt, find SQLSetStmtAttrW and SQLGetStmtAttrW, compare prologues.
```

Look for: stack-cookie checks (`__security_cookie`), `__chkstk`, mismatched
prologue/epilogue, or a jump to a thunk that lands outside the DLL.

---

## STEP 5 — Apply the fix

Driven by the above:
- Static-CRT culprit  -> ship the dynamic-CRT build (or x64-windows-static-md).
- DM dispatch/handle  -> fix handle registration so the DM's map resolves correctly.
- Bad function thunk  -> adjust export (NONAME/ordinals) or calling convention.
