# Resource Routing Stage Telemetry V2

Diagnostic-only successor to Recovery V1. It changes no renderer equations or resource bind paths.

The periodic line exposes the preserved V8/V12 one-shot stage latches:

- CS = CAPTURE_SPEC
- CN = CAPTURE_NORMAL
- CD = CAPTURE_DIFFUSE
- DG = DIFFUSE_GATE
- DS = DIFFUSE_SIDECAR_READY
- DIF = DIFFUSE_T0_BIND
- NG = NORMAL_GATE
- NS = NORMAL_SIDECAR_READY
- NRM = NORMAL_T2_BIND

Interpret the first zero after earlier ones as the current activation boundary. SPC, ENV, SUB, B12C, B12H and FAIL remain available.

This diagnostic exists because owner runtime falsified Recovery V1 as a sufficient fix: liveness and SPC remained good, but DIF/NRM stayed zero. Do not infer pixel equivalence from activation.
