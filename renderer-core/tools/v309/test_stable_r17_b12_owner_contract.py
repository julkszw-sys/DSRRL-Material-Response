#!/usr/bin/env python3
"""V309 R17-vs-R19 b12 ownership guard (source/ABI; pixel status OPEN).
No retail DXBC, no guessing buffer sizes and no runtime feature changes.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
src=lambda p:(ROOT/p).read_text(encoding="utf-8")
stable=src("src/operators/env_spec/pmetal_rgba_materializer.cpp")
mr=src("src/operators/material_response/material_response_diffuse_v1.cpp")
lerp=src("src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
runtime=src("src/runtime/pmetal_envspec_draw_runtime.cpp")
export=src("tools/v300/export_real_r17_final_dxbc.cpp")
checks={
 "stable_rdef_64":'"DSRRL_MaterialCarrier",\n                12u,\n                64u)' in stable,
 "mr_base_64":'"DSRRL_MaterialCarrier",\n                12u,\n                64u)' in mr,
 "mr_dcl_four":'0x00000004u' in mr,
 "lerp_rdef_96":'k_material_carrier_bytes = 96u;' in lerp,
 "lerp_resize_64_to_96":'existing_size != 64u' in lerp and
   'resize_existing_material_carrier_rdef(' in lerp,
 "runtime_upload_96":'desc.ByteWidth = 96u;' in runtime and
   'static_assert(sizeof(payload) == 96u);' in runtime,
 "stable_export_64":'cb.Size==64' in export and 'RDEF_b12=64' in export,
 "stable_export_not_96":'cb.Size==96' not in export,
 "lerp_uses_6_vectors":'std::array<f4,6> payload' in runtime,
}
for name,ok in checks.items():
 print(name, "PASS" if ok else "FAIL")
 if not ok:raise SystemExit(2)
print("V309_STABLE64_LERP96_RUNTIME96_ABI_SOURCE_PASS")
