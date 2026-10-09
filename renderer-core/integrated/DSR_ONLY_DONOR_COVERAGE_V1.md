# DSR-only DrawParam donor compatibility

Exact original DSR/PTDE pairwise census: 323 DSR-only rows across 35 bank names. The three extra rows in active P_Metal LightBank banks are m14:64, s14:64, m18:64; these use original DSR RGBM. 320 DSR-only rows belong to complete default/m99/s99 banks not found in PTDE reference and retain the native DSR producer/operator rather than fake PTDE donors.

The R43 `DSREDV01` PTDE EnvDiffuse sidecar v1 is preserved byte-for-byte (1246 PTDE rows); a dedicated exact DSR-only row64 overlay operates outside that v1 binary. The generated P_Metal EnvSpec authority retains the 1246 PTDE rows and adds the three original DSR extras. PointLight's ten paired active banks contain no row gaps, and its default/m99 banks remain native DSR.

Source data derived from original uploaded Vanilla DSR DrawParam and DSR ORIGINAL PTDE SWITCH DrawParam. Do not promote to pixel equivalence without owner validation. No R10E is inherited.
