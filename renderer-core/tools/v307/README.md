# V307 — PowerShell Python invocation fix in private V300/V301 owner runner

The V302 private runner preferred `py` whenever it existed and evaluated:

```powershell
$pyargs=if($py){@('-3')}else{@()}
Invoke-Check -prog $pyexe -Params ($pyargs + $Params)
```

Windows PowerShell 5.1 unwraps the one-element `@('-3')` into a scalar string; the `+` operation concatenated `-3` and the script path, passing one invalid version selector such as `-3C:\\Users\\...\\VERIFY_V302_PACKAGE.py`. The user already has Python installed; do **not** install or ship another interpreter.

V307 owner-private fixed runner selects an actual executable `python.exe`, then `python3.exe`, then `py.exe -3` only if needed; probes each with `-c` to verify Python 3. It constructs a proper `[string[]]` argument vector with separate array appends and calls `& $pythonExe @argv`. Every existing native binary and original DXBC remains byte-for-byte unchanged. Verified locally: ZIP CRC valid, only `RUN_V300_V301_ON_WINDOWS.ps1` and `README_OWNER_PRIVATE.txt` changed; original nine pinned executable/DXBC entries and manifest unchanged. Windows-native execution by owner: OPEN.

Correct command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\\RUN_V300_V301_ON_WINDOWS.ps1
```

`-NoProfile` prevents the unrelated profile.ps1 policy warning.

Private V307 ZIP SHA256: `e5fbb318af790239c37d24344cfa7216dd66faaac0994efeca44220afe127880`.

Fog stock DSR, `latest_hook_source` PROTECT, R43 RC1 immutable. No shader materialization or pixel status is promoted by this launcher repair.