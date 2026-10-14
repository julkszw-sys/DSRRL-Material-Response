# V308 — fix false Python 3 detection on owner Windows runner

Owner has **Python 3.14.7**: `python --version` works in the same PowerShell session. V307 nevertheless reported "Installed Python 3 was not callable", because the interpreter detector ran a fragile nested-quote `python -c` expression through Windows PowerShell 5.1 and suppressed its errors. Do not suggest installing Python, changing PATH, adjusting ExecutionPolicy system-wide or adding a portable Python runtime.

The private owner-side standalone V308 script **replaces the probe with precisely `& python --version`** (requires exit code 0 and output `Python 3.x[.y]`), and invokes each Python script directly as separate PowerShell native arguments using `& python @Params`. No `py.exe`, no `-3`, no inline `-c`, no extra dependencies.

- Native regression: [Windows PowerShell 5.1, Python 3.14 Actions run 38546918741](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/38546918741), PASS including Python script path with spaces.
- Package integrity: source V307 ZIP and V308 ZIP have exactly identical files other than `RUN_V300_V301_ON_WINDOWS.ps1` and `README_OWNER_PRIVATE.txt`. All 3 native EXE, 6 original DXBC, SHA manifests are byte-identical. CRC PASS.
- Python preflight tests `VERIFY_V302_PACKAGE.py`, `TEST_V301_ASSEMBLER_NO_ASSETS.py`, V288, V297: PASS locally.
- Standalone patched script SHA256 `39560a8b07fa978f2d8a8d3598f68480c24cc4aea385dd9e9e954ecc0d02bbb5`.
- Owner-private ZIP SHA256 `d8025e6195251740cd17a322c106a598ed52ef3afc72c3b7bcff2aef014a0f37`.

To use with **existing V307 contents**, save standalone `RUN_V308_OWNER_PYTHON_FIXED.ps1` beside the original V307 script and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V308_OWNER_PYTHON_FIXED.ps1 -PythonProbeOnly
powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_V308_OWNER_PYTHON_FIXED.ps1
```

V308 `-PythonProbeOnly` invokes the packaged SHA validation without executing any native DXBC exporters. Successful preflight is **not** PTDE renderer activation. R43 RC1, stock Fog and Renderer PROTECT `latest_hook_source` remain unchanged. Full native owner-final R17/R19/V286/V297 materialization on the user's Windows is still unobserved.
