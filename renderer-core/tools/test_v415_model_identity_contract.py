#!/usr/bin/env python3
"""V415 exact native model identity observation construction contract."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
p=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
h=(root/"include/dsrrl/runtime/v415_model_draw_identity.hpp").read_text("utf-8")
b=(root/"tools/build_v415_model_identity_diagnostic.ps1").read_text("utf-8")
w=(root.parent/".github/workflows/v415-flver-model-identity.yml").read_text("utf-8")

# Function address and class ABI are exact original EXE disassembly / RTTI.
assert "rva==k_ret_sel_1" in s
assert "reinterpret_cast<std::uintptr_t>(container)-0x88u" in s
assert "model_vtable==g_base+0x12EB130u" in s
assert "model_vtable==g_base+0x12EB760u" in s
assert "range_ok(model_ptr,sizeof(model_vtable))" in s
assert "[DSRRL V415 MODEL SELECTOR]" in s
assert "v415::reset_selector()" in s
assert s.index("v415::reset_selector()")<s.index("if(g_base==0u || ret==nullptr || material_index<0)")
assert s.index("v415::record_selector(")<s.index("observe_model_lightbank_source(" ,s.index('extern "C" void dsrrl_flver_selector_observer('))
assert "[DSRRL V415 MODEL SOURCE JOIN]" in p
assert "v415::current_for(" in p
assert "v415::current_for(" in p[p.index("const bool complete_v411"):p.index("const bool journaled=v411::observations().observe(w);")]
assert "thread_local model_draw_stamp" in h
assert "model_this!=0u" in h and "s.context==context" in h
assert "DSRRL_V415_FLVER_MODEL_ID_DIAG" in b
assert "/DDSRRL_V415_FLVER_MODEL_ID_DIAG" in b
assert "DSRRL_V415_Model_Identity_Diagnostic.addon64" in b
assert "DSRRL_V415_Model_Identity_Diagnostic.addon64" in w
assert "build_v415_model_identity_diagnostic.ps1" in w
# These code changes do not touch the Source1/Source2 selection routine.
assert "bool latest_hook_source(" in p
assert "if (latest_hook_source(out))" in p
assert "pmetal_producer_state_publish(" in p
assert "native_actor=OPEN pixel=OPEN" in s
assert "actor_lifetime=OPEN gpu_join=OPEN pixel=OPEN" in p
assert "g_selector_model" not in p[p.index("bool latest_hook_source("):p.index("void pmetal_env_source_selector_event(")]
print("V415 exact model selector-to-native-sequence diagnostic CONTRACT PASS; actor/pixel OPEN")
