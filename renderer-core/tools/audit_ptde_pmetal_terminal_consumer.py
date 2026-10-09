#!/usr/bin/env python3
"""Read-only exact PTDE P_Metal PS3 terminal consumer audit (no asset redistribution)."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED = {
    "FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo": "0c6f2982bc36d56151791acd03caffb63358324d746f3dd408cfeda976fb2daf",
    "FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo": "9e931f72ca7ebe7ae5bf68f23ff8a7ff3a818f314fd2578f47b1315e9331d9a6",
    "FRPG_Phn_DifSpcBmp______Csd_HemEnvLerp.fpo": "ab3debbe2f90ccbcc7b70173dc87b4c98c6b6e126b1bd6d93b59685fd38ba72d",
    "FRPG_Phn_DifSpcBmp______Sdw_HemEnvLerp.fpo": "51451c962c42574a1335a5ebe0eadbc1bc7e3929bcf4af7684cc8229e81d875f",
}

def audit_shader(blob):
    if len(blob) < 16 or len(blob) % 4:
        raise ValueError("invalid shader byte length")
    words = struct.unpack("<%dI" % (len(blob)//4), blob)
    if words[0] != 0xffff0300:
        raise ValueError("not DX9 ps_3_0")
    pos = 1
    ops = []
    while pos < len(words):
        op = words[pos] & 0xffff
        if op == 0xffff:
            if pos != len(words)-1:
                raise ValueError("shader has trailing instructions")
            break
        width = 1+(((words[pos] >> 16) & 32767) if op == 0xfffe else ((words[pos] >> 24) & 15))
        if pos+width > len(words):
            raise ValueError("truncated shader")
        if op != 0xfffe:
            ops.append((pos,op,words[pos:pos+width]))
        pos += width
    else:
        raise ValueError("missing ps_3_0 END")
    if len(ops) < 3:
        raise ValueError("missing PTDE PHN terminal")
    mul,rcp,out=ops[-3:]
    if [x[1] for x in (mul,rcp,out)] != [5,6,5]:
        raise ValueError("terminal opcodes are not MUL RCP MUL")
    if [len(x[2]) for x in (mul,rcp,out)] != [4,3,4]:
        raise ValueError("terminal instruction widths differ")
    if mul[2][1] != 0x80270000 or rcp[2][1] != 0x80080000 or out[2][1] != 0x80370800:
        raise ValueError("terminal output semantics differ")
    if mul[2][-1] != 0xa0000087 or rcp[2][-1] != 0xa0550087:
        raise ValueError("terminal k135 numerator/denominator mismatch")
    return {"terminal_words": [mul[0],rcp[0],out[0]],
            "operator": "sat((PTDE_c135.x/PTDE_c135.y)*preterminal_rgb)",
            "consumer_identity": "CONFIRMED",
            "producer_identity": "OPEN",
            "pixel_equivalence": "UNVERIFIED"}

def main():
    cli=argparse.ArgumentParser()
    cli.add_argument("shader_directory", nargs="?", type=Path)
    cli.add_argument("--selftest", action="store_true")
    cli.add_argument("--output", type=Path)
    args=cli.parse_args()
    if args.selftest:
        body=[0xffff0300,0x03000005,0x80270000,0x80e40002,0xa0000087,
              0x02000006,0x80080000,0xa0550087,
              0x03000005,0x80370800,0x80ff0000,0x80e40000,0x0000ffff]
        sample=struct.pack("<%dI"%len(body),*body)
        assert audit_shader(sample)["consumer_identity"]=="CONFIRMED"
        body[7]=0xa0550088
        try:
            audit_shader(struct.pack("<%dI"%len(body),*body))
        except ValueError:
            print("PASS PTDE terminal and incorrect c135 operand fail closed")
            return
        raise AssertionError("invalid carrier was accepted")
    if args.shader_directory is None:
        cli.error("shader_directory required except in --selftest")
    result={"reference":"Original PTDE PHN PS3 exact SHA256", "conclusion":"consumer confirmed, producer unknown",
            "runtime_patch_allowed":False,"shaders":{}}
    for name,expected in EXPECTED.items():
        data=(args.shader_directory/name).read_bytes()
        sha=hashlib.sha256(data).hexdigest()
        if sha!=expected:
            raise ValueError(f"{name} not exact PTDE original: {sha}")
        result["shaders"][name]={"sha256":sha,**audit_shader(data)}
    output=json.dumps(result,ensure_ascii=False,indent=2)+"\n"
    if args.output:
        args.output.write_text(output,encoding="utf-8")
    else:
        print(output,end="")

if __name__=="__main__":
    main()
