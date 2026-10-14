# V317: V272 equipment baseline + exact owner V286 Lerp

The base commit is ae9ac314d06ad8758e1aa7430cb236eef0eefe27 (owner-verified V272). Preserve all equipment SpecRGB paths and material consumers. Supersede only the legacy R19 P_Metal HemEnvLerp shader registration with the exact V286 RX33/34/35 sidecars, keeping DSR Fog and LightScattering intact. Any source or sidecar hash mismatch must fail open to stock DSR, never to the obsolete Lerp shader. Native addon construction and in-game activation are separate tests.
