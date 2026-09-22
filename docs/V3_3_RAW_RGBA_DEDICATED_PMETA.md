# V3.3 Raw RGBA + Dedicated P_Metal Consumer

Runtime successor to V3.2 after owner black-armor falsifier.

Construction target:
- preserve V3.2 build131 coexistence callsite routing;
- preserve exact native DSR probe fingerprint -> PTDE probe ordinal mapping;
- for PTDE_PRESENT, do not predecode each texel to R11G11B10 before filtering;
- materialize the canonical PTDE 32x32x6 one-mip carrier as raw RGBA8 so filtering happens on RGBA and RGB/sample-alpha decode remains in the dedicated build131 consumer;
- exact P_Metal may activate PTDE_PRESENT only when the bound consumer is one of build131 dedicated DXBC72/73/74;
- P_Metal on shared retail receivers fails open;
- EXPLICIT_NONE remains limited to the existing certified safe subset;
- no arbitrary gain and no PointLight changes.

Construction/runtime/pixel status are independent. This document does not claim runtime or pixel success.
