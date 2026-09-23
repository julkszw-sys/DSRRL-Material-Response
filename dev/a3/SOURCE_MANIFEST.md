# A3 source/input manifest

All hashes are SHA-256.

| Role | Identity |
|---|---|
| Shipping Material Response 1.45 legacy basis | `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342` |
| 1.45 exact integrated pre-release basis (historical input to public release builder) | `db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966` |
| 1.45 clean intermediate | `3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000` |
| A1 source `material_response_expanded.cpp` | `18dbf46e54ad7ffcafa4310bccbf191381166544bcbfeeb2a9b13ca5f8a8b24a` |
| A1 generated closed plan header | `4a3ff0c9b66d78de158b3324d9351d827ba5db1f889114e631ebe7368d2c9e60` |
| A2 source `a2_receiver_telemetry.cpp` | `b919c490c7efc4c440add953e6102f49d03c371300aa4ba8958fbfc50fae0011` |
| A2 generated receiver census header | `286bd370d2b3850ac3c20ba44b730646737f30461789dc3d956d9ffce604eddc` |
| R1/R1T/R2D full closed-plan header used as A3 planning corpus | `99e16440431687b1d195954ca6b688ecdfde4f8bd2ac4233082738ece2324c7a` |
| A3 U/L generator snapshot | `8f5ca6503ad3c43597f44a4251d4a3b3021f93378e57546345551c4c0a13db3f` |
| A3 U/L host table snapshot | `b1d391f8932ff7792448a14b312605998ab54b74017bd2b40b3f86f1d5a7f804` |
| A3 U/L payload audit snapshot | `76c268b664e2a2f494a9a7286af335225912631f59a50261937c01869e37bb24` |
| ReShade source basis | commit `aae2b7ecc18096ccddca2c073b50727541220292` |
| ReShade API | 20 |

## Current generated U/L payload population

- 48 generic verified HemEnv/HemEnvLerp hosts.
- 3 P_Metal exact shipping-1.45 alternatives for receiver indices 894 / 913 / 932.
- PTDE U/L consumer transport: pixel-shader `b13[6].xyz` = Upper, `b13[7].xyz` = Lower.
- Generic U/L payloads are generated from the certified vanilla host shaders.
- P_Metal U/L payloads are generated from the shipping 1.45 embedded alternatives, then patched for U/L only, so this path does not intentionally roll back the shipping P_Metal material response.

## Non-goals in current A3

- no PTDE EnvDiffuse resource replacement yet;
- no PTDE EnvSpec cubemap/resource replacement;
- no blanket shared-body visible patch for P_Metal-capable DifSpcBmp hosts;
- no PointLight gain/range retuning.
