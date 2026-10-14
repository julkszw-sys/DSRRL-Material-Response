#!/usr/bin/env python3
"""Private reproducible V315 sidecar assembly from user's exact V300 archive.
No owner shader DXBC is stored in GitHub. Stock Fog/LightScattering are retained.
"""
import argparse, hashlib, json, pathlib, zipfile

V286 = {
    33: "07be4be60e2fd940ba5dd2ca65114d69b4c027b3770dfee4304d86988bb96be0",
    34: "fd4de199491cf2e46355fecb9a70df1db643e2862a28a3204eecf5f7ff10d2a0",
    35: "a1b19e53200568b94c81885c6616490d68d9f012607e527b5f69e1f57871b8ff",
}
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("original_owner_v300_zip",type=pathlib.Path)
    ap.add_argument("output_directory",type=pathlib.Path)
    args=ap.parse_args()
    root=args.output_directory
    if root.exists(): raise ValueError("new output path required; never overwrite")
    stage=[]
    with zipfile.ZipFile(args.original_owner_v300_zip) as archive:
        for rx, expected in V286.items():
            name=f"V300_OWNER_RESULT/v286/v286_R5_R11_R7_LerpRX{rx}.dxbc"
            blob=archive.read(name)
            observed=hashlib.sha256(blob).hexdigest()
            if observed!=expected or blob[:4]!=b"DXBC":
                raise ValueError(f"Owner RX{rx} failed exact V286 SHA/DXBC")
            post=archive.read(f"V300_OWNER_RESULT/v297_postls/v286_R5_R11_R7_LerpRX{rx}.dxbc")
            if len(post)!=len(blob) or post==blob:
                raise ValueError(f"Post-LS lineage not distinguishable for RX{rx}")
            stage.append((rx,blob,observed))
    folder=root/"DSRRL"/"EnvSpec"/"HemEnvLerp"
    try:
        folder.mkdir(parents=True)
        for rx,blob,_ in stage:
            (folder/f"v286_RX{rx}.dxbc").write_bytes(blob)
        (root/"V315_SIDECAR_MANIFEST.json").write_text(json.dumps({
            "schema":"DSRRL_V315_PRIVATE_OWNER_V286_STOCK_FOG_LS_V1",
            "operator_policy":{"fog":"STOCK_DSR","lightscattering":"STOCK_DSR",
                               "custom_post_ls":"OFF"},
            "file_root_relative_to_game_exe":"DSRRL/EnvSpec/HemEnvLerp",
            "source":"V300_OWNER_RESULT original owner private archive",
            "sidecars":[{"rx":rx,"sha256":sha,"size":len(blob),
                         "name":f"v286_RX{rx}.dxbc"} for rx,blob,sha in stage],
            "native_addon":"SEPARATE_CI_ARTIFACT","runtime":"OPEN","pixel":"OPEN"
        },indent=2)+"\n",encoding="utf-8")
    except Exception:
        import shutil
        shutil.rmtree(root,ignore_errors=True)
        raise
    print("V315_EXACT_OWNER_V286_RX33_35_SIDECARS_PASS; STOCK_FOG_LS; RUNTIME_OPEN")
if __name__=="__main__":main()
