#!/usr/bin/env python3
"""V280 Stable HemEnv <-> HemEnvLerp source/receiver continuity CONTRACT.

This is a build-time *coverage* gate, not a PTDE pixel continuity proof.
Status OPEN is intentional: source-level similarity cannot prove the
endpoint identities, live PTDE GPU signals or complete physical slot owner.

Verify R17 stable P_Metal control remains a separate prior owner pixel PASS.
Do not activate stable-only body bridge (project-wide PROTECT).
"""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[3]
STABLE=ROOT/"renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp"
LERP=ROOT/"renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"
STABLE_CENSUS=ROOT/"renderer-core/data/census/stable_hemenv_receiver_census_v1.json"
REFERENCE=ROOT/"renderer-core/tools/v257/ptde_equipment_hemenvlerp_core.hpp"

def require(condition,why):
    if not condition:
        raise RuntimeError("V280_STABLE_LERP_OPERATOR_CONTINUITY_REGRESSION "+why)

def run():
    a=STABLE.read_text(encoding="utf8")
    b=LERP.read_text(encoding="utf8")
    r=REFERENCE.read_text(encoding="utf8")
    census=json.loads(STABLE_CENSUS.read_text(encoding="utf8"))
    entries=census.get("entries",[])
    stable_rx={e["receiver_id"]:e for e in entries}
    require(len(stable_rx)==len(entries) and {33,34,35}.issubset(stable_rx),
            "original stable receiver census lacks exact Csd/Sdw/plain pairing")
    require(all(stable_rx[x]["shader_name"].endswith("_HemEnv.fpo")
                for x in (33,34,35)),"stable shader identity not HemEnv")
    require("diffuse_v1_family::" in a and "stable_hemenv" in a,
            "stable shader material family guard missing")
    require("diffuse_v1_family::hemenvlerp" in b,
            "Lerp shader material family guard missing")
    require("materialize_ptde_diffuse_response_v1(" in a and
            "materialize_ptde_diffuse_response_v1(" in b,
            "shared diffuse-base producer gate removed")

    # These statuses are not proof the operations are equivalent:
    # they name the real producer/consumer implementation path.
    rows=[
      ("diffuse_base","materialize_ptde_diffuse_response_v1(",
       "materialize_ptde_diffuse_response_v1(","COMMON_STAGE_PRESENT_INPUT_PARITY_OPEN"),
      ("EnvSpec","apply_build131(","k_ptde_rgba_envspec_chain",
       "SEPARATE_PTDE_CARRIERS_ENDPOINT_PARITY_OPEN"),
      ("EnvDiffuse","apply_exact_ptde_envdiffuse_consumer(",
       "apply_exact_ptde_envdiffuse_lerp_consumer(",
       "STABLE_ONE_ENDPOINT_VS_LERP_TWO_ENDPOINTS_IDENTITY_OPEN"),
      ("material_workflow","apply_build131(",
       "apply_exact_ptde_lerp_materialworkflow_r15_r16_r17(",
       "SOURCE_LOCAL_OPERATOR_PRESENT_FULL_EQUATION_PARITY_OPEN"),
      ("SpecRGB","outcome.spec_rgb_consumer = true;",
       "materialize_spec_rgb_consumer(",
       "DIFFERENT_CONSUMER_FAMILIES_OWNER_AND_MATERIAL_RESPONSE_OPEN"),
      ("PHN_terminal","compose_exact_phn_scene_encoding(",
       "apply_exact_phn_scene_encoding(",
       "CONSUMER_PRESENT_K135_PRODUCER_AND_PIXEL_OPEN"),
      ("normal_basis_R5","apply_exact_ptde_normal_basis_r5(",
       None,"STABLE_EXPLICIT_LERP_EQUIVALENT_NOT_IDENTIFIED"),
      ("Csd_Sdw_visibility_R7","apply_exact_ptde_shadow_visibility_r7(",
       None,"STABLE_EXPLICIT_LERP_EQUIVALENT_NOT_IDENTIFIED"),
      ("postmerge_visibility_R3","remove_dsr_postmerge_visibility_scalar_r3(",
       None,"STABLE_EXPLICIT_LERP_EQUIVALENT_NOT_IDENTIFIED"),
      ("postmerge_SAO_R11","remove_dsr_sao_surface_multiplier_r11(",
       None,"STABLE_EXPLICIT_LERP_EQUIVALENT_NOT_IDENTIFIED")
    ]
    checks=[]
    for name,stable_call,lerp_call,status in rows:
        require(stable_call in a,"stable baseline cut missing: "+name)
        if lerp_call:
            require(lerp_call in b,"Lerp counterpart missing: "+name)
        else:
            # Absence of the *named call* does not prove missing behavior;
            # some Lerp operators may be represented differently.
            require(stable_call not in b,
                    "Lerp acquired stable call: review routing/parity "+name)
        checks.append({
            "cut":name,"stable_source_call":stable_call,
            "lerp_source_call":lerp_call,
            "source_structure_verified":True,
            "semantic_or_pixel_parity":status
        })
    for token in ("struct environment","struct surface_input","compose_local(",
                  "compose_terminal(","env_blend("):
        require(token in r,"PTDE reference kernel removed: "+token)
    require("output = std::move(base)" in a and
            "output = std::move(spec_rgb_base)" in b,
            "source materializer outcome path changed")
    assert not any(x["semantic_or_pixel_parity"]=="PIXEL_PASS" for x in checks)
    result={
      "schema":"dsrrl.v280.stable_lerp_operator_continuity_source_contract.v1",
      "status":"SOURCE_STRUCTURE_PASS_PARITY_NOT_ESTABLISHED",
      "stable_control":"owner-authenticated R17 P_Metal HemEnv rx33/34/35 only; historical visible success",
      "lerp":"R19 reconstructed; new replacement RX shader binding still dormant",
      "stable_original_rx":[{k:stable_rx[r][k] for k in
            ("receiver_id","shader_name","code_sha256")} for r in (33,34,35)],
      "operator_cuts":checks,
      "open_endpoints":[
          "exact same physical equipment FLVER/MTD/material slot and PTDE sidecar",
          "Lerp(beta=0) at A endpoint versus stable(A) on matched source",
          "Lerp(beta=1) at B endpoint versus stable(B) on matched source",
          "shared normal basis and visibility Csd/Sdw equivalence by receiver",
          "matched preFog RGB then separate stock Fog/LS/Glow and terminal",
          "actual receiver PS active and PTDE-visible pixel continuity"],
      "forbidden_promotions":[
          "stable R17 P_Metal pixel success => Lerp pixel success",
          "stable-only Ps_Body Subsurface visible binding",
          "stock RX shared shader => matching equipment material owner",
          "V278 extra COLOR0 MUL without complete original per-channel trace"]
    }
    print("V280_STABLE_HEMENV_LERP_SOURCE_CONTINUITY_CENSUS_PASS",
          len(entries),"stable_receivers",len(checks),"operator_cuts")
    print("V280_OPERATOR_EQUIVALENCE_AND_PIXEL_CONTINUITY_OPEN")
    return result

if __name__=="__main__":
    run()
