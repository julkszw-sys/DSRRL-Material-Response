#!/usr/bin/env python3
"""V256: reproducible *static* v2.0.3 -> post-V255 P_Metal LERP source audit.

Only reads tracked Git commits and invokes V236/V245 source overlays in a
disposable temporary directory. Does not install, invoke, or patch the game,
does not create an addon, and deliberately does not change consumer selection.
Outputs line-precise JSON and human-readable unified diff reports.
"""
import argparse
import difflib
import hashlib
import json
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
V203 = "6a73033f6275f88487effeb1e34bbbdd7a6a3a88"
V255 = "4ea4e623bed1900c8470b0e6304d48ae3f09dfc8"

FILES = {
  "lerp_materializer": "renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp",
  "pmetal_source": "renderer-core/src/runtime/pmetal_env_source_runtime.cpp",
  "pmetal_producer": "renderer-core/src/runtime/pmetal_producer_state.cpp",
  "pmetal_draw": "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp",
  "flver_selector": "renderer-core/src/runtime/flver_engine_hooks.cpp",
  "env_resources": "renderer-core/src/runtime/envspec_resource_runtime.cpp",
  "material_resources": "renderer-core/src/runtime/material_resource_draw_runtime.cpp",
  "native_draw_transaction": "renderer-core/src/runtime/draw_state_transaction.cpp",
  "integrated": "renderer-core/integrated/integrated_addon.cpp",
  "cmake": "renderer-core/integrated/CMakeLists.txt",
}

OPERATOR_UNCHANGED = (
  "lerp_materializer", "pmetal_source", "pmetal_producer",
  "env_resources", "material_resources",
  "native_draw_transaction", "integrated",
)

def git_show(rev, path):
    return subprocess.check_output(
        ["git", "-C", str(ROOT), "show", f"{rev}:{path}"]
    ).decode("utf-8").replace("\r\n", "\n")

def sha(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()

def changes(before, after):
    a, b = before.splitlines(keepends=True), after.splitlines(keepends=True)
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    out = []
    for tag, ia, ja, ib, jb in sm.get_opcodes():
        if tag == "equal": continue
        out.append({
            "operation": tag, "old_start_line": ia+1,
            "old_end_line": ja, "new_start_line": ib+1,
            "new_end_line": jb,
            "old_lines": "".join(a[ia:ja]), "new_lines": "".join(b[ib:jb]),
        })
    return out

def diff(before, after, label_a, label_b):
    return "".join(difflib.unified_diff(
        before.splitlines(keepends=True),
        after.splitlines(keepends=True),
        fromfile=label_a, tofile=label_b, n=5
    ))

def overlay_v236_v245(base):
    path = pathlib.Path(FILES["pmetal_producer"])
    with tempfile.TemporaryDirectory() as tmp:
        home = pathlib.Path(tmp)
        target = home / path
        target.parent.mkdir(parents=True)
        target.write_text(base, encoding="utf8")
        for tool in ("apply_v236_slot_sync.py",
                     "apply_v245_exact_source_record_rearm.py"):
            result = subprocess.run(
                ["python3", str(ROOT/"renderer-core"/"tools"/tool), "--apply"],
                cwd=home, capture_output=True, text=True
            )
            if result.returncode:
                raise RuntimeError(tool+" failed: "+result.stdout+result.stderr)
        output = target.read_text(encoding="utf8")
        assert "std::lock_guard<std::mutex> lock(slot.mutex);" in output
        assert "a.envdiffuse_a == b.envdiffuse_a" in output
        assert "a.phn_k135_valid == b.phn_k135_valid" in output
        assert "same_source_payload(slot.record.source, g_record.source)" in output
        # Re-running source-only transforms should not change semantics.
        second = subprocess.run(
            ["python3", str(ROOT/"renderer-core"/"tools"/"apply_v245_exact_source_record_rearm.py")],
            cwd=home, capture_output=True, text=True
        )
        if second.returncode:
            raise RuntimeError("V245 idempotence failed: "+second.stderr)
        return output

def inspect_reference_guards():
    base = ROOT/"renderer-core"/"tools"
    v216 = (base/"apply_v216_bound_t1_native_alias.py").read_text(encoding="utf8")
    v224 = (base/"apply_v224_native_probe_resource_join.py").read_text(encoding="utf8")
    v253 = (base/"apply_v253_ptde_envspec_rgbm.py").read_text(encoding="utf8")
    v255 = (base/"apply_v255_lerp_phn_cut_diag.py").read_text(encoding="utf8")
    assert "sidecar_20261010::exact_nonmetal_material(query.material)" in v216
    assert "sidecar_20261010::exact_nonmetal_draw(material, decision)" in v224
    assert "ptde_spec_exact(" in v253
    assert "FAKE_REPLACEMENT" in v255
    return {
        "v216_t1_alias": "only exact nonmetal material and RX33-35; check global registry effects separately",
        "v224_t12_resource_join": "only exact nonmetal draw on resource miss; not directly P_Metal",
        "v253_spec_rgbm": "alters P_Metal source endpoint RGB; tested bank row5/6 near equal in owner log",
        "v255_observer": "log-only; P_Metal event metadata and phn expected candidate, no operator rewrite",
        "caveat": "Guard inspection is source-level, not a runtime or pixel proof"
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--out",default="audit-output/v256")
    opts=ap.parse_args()
    dest=ROOT/opts.out
    dest.mkdir(parents=True, exist_ok=True)
    report={
      "baseline":V203,"current":V255,
      "status":"STATIC_CODE_ONLY; no gameplay or PTDE pixel inference",
      "files":{},"scope_checks":inspect_reference_guards(),
      "source_overlays":{}
    }
    combined=[]
    for key, path in FILES.items():
        old=git_show(V203,path)
        now=git_show(V255,path)
        hunks=changes(old,now)
        if key in OPERATOR_UNCHANGED and old!=now:
            raise RuntimeError("unexpected tracked operator drift: "+path)
        if key == "pmetal_draw":
            for h in hunks:
                d=h["old_lines"]+h["new_lines"]
                if ("v203_equipment" not in d and
                    "DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG" not in d):
                    raise RuntimeError("unaccounted P_Metal draw static hunk: "+str(h["old_start_line"]))
        if key == "flver_selector":
            for h in hunks:
                d=h["old_lines"]+h["new_lines"]
                if ("v203_equipment" not in d and
                    "DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG" not in d):
                    raise RuntimeError("unaccounted FLVER static hunk: "+str(h["old_start_line"]))
        report["files"][key]={
            "path":path,"old_sha256":sha(old),"new_sha256":sha(now),
            "identical":old==now,"changed_hunks":len(hunks),
            "hunks":hunks if key in ("pmetal_draw","flver_selector") else []
        }
        if hunks:
            combined.append(diff(old,now,"v203/"+path,"v255/"+path))
    pold=git_show(V203,FILES["pmetal_producer"])
    pnew=overlay_v236_v245(pold)
    report["source_overlays"]["v236_then_v245"]={
        "unpatched_sha256":sha(pold),"patched_sha256":sha(pnew),
        "hunks":changes(pold,pnew),
        "affected_units":["same_source_payload","source_key",
                          "pmetal_producer_state_begin",
                          "pmetal_producer_state_publish"],
        "does_not_rewrite":"R19 shader/PS CB register mapping/EnvSpec texels"
    }
    combined.append(diff(pold,pnew,
        "v203/pmetal_producer_state.cpp",
        "v255_effective/V236_V245_pmetal_producer_state.cpp"))
    (dest/"source_hunks.json").write_text(
        json.dumps(report,indent=2,ensure_ascii=False),encoding="utf8")
    (dest/"line_by_line.diff").write_text(
        "\n".join(combined),encoding="utf8")
    s=[
      "# V256 — v2.0.3→V255 static line-level P_Metal audit",
      "",
      f"Source commits: {V203} -> {V255}. No owner runtime needed.",
      "",
      "Status: STATIC; does not prove a pixel regression cause.",
      "",
      "| Component | Tracked code unchanged? | Number of diff hunks |",
      "|---|---|---:|"
    ]
    for name,item in report["files"].items():
        s.append(f"| {name} | {'YES' if item['identical'] else 'NO'} | {item['changed_hunks']} |")
    s+=["","## Exact renderer cut",
        "Tracked R19 materializer, producer, native draw transaction,",
        "resource handlers and integrated source are byte-identical at both commits.",
        "P_Metal draw/FLVER tracked differences have only V203 exact nonmetal gates.",
        "V236/V245 generated overlay modifies *producer state caching/synchronization*;",
        "it does not alter R19 bytecode or b12 register ABI.",
        "V216 and V224 have exact nonmetal guards; their global resource registry",
        "lifetime and side effects are not proven equivalent here.",
        "",
        "No global LERP output gain is justified by this static report.",
        "For exact changed line content see line_by_line.diff / source_hunks.json.",
    ]
    (dest/"README.md").write_text("\n".join(s)+"\n",encoding="utf8")
    print("V256_STATIC_V203_V255_SCOPE_AUDIT_PASS")
    print(json.dumps({k:(v["identical"],v["changed_hunks"]) for k,v in report["files"].items()}))

if __name__=="__main__":
    main()
