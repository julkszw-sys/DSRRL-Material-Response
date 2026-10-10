#!/usr/bin/env python3
"""Deterministic offline tests for exact stock compressed DDS TPF fingerprints."""
import hashlib
import struct
from verify_spc25_dsr_tpf_dds_fingerprints_offline import dds_fingerprint, parse_tpf

def make_dds(fourcc=b"DXT1", width=4, height=4, payload=b"abcdefgh"):
    header = bytearray(148 if fourcc == b"DX10" else 128)
    header[0:4] = b"DDS "
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 12, height)
    struct.pack_into("<I", header, 16, width)
    struct.pack_into("<I", header, 28, 1)
    header[84:88] = fourcc
    if fourcc == b"DX10":
        struct.pack_into("<IIIII", header, 128, 83, 3, 0, 1, 0)
    return bytes(header) + payload

def reject(callback, description):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError("Expected rejection: " + description)

bc1 = make_dds()
identity = dds_fingerprint(bc1)
assert (identity["width"],identity["height"],identity["mips"],
        identity["dxgi_format"],identity["bytes"]) == (4,4,1,71,8)
assert identity["gpu_mip_bytes_sha256"] == hashlib.sha256(b"abcdefgh").hexdigest()
assert dds_fingerprint(bc1[:-1]+b"i")["gpu_mip_bytes_sha256"] != identity["gpu_mip_bytes_sha256"]
reject(lambda: dds_fingerprint(bc1[:-1]), "truncated mip")
reject(lambda: dds_fingerprint(make_dds(width=8)), "invalid mip block count")
reject(lambda: dds_fingerprint(make_dds(fourcc=b"XXXX")), "unsupported compression")

bc5 = dds_fingerprint(make_dds(b"DX10",payload=b"\x1c" * 16))
assert bc5["dxgi_format"] == 83 and bc5["bytes"] == 16
dx10bad = bytearray(make_dds(b"DX10",payload=b"\x1c" * 16))
struct.pack_into("<I", dx10bad, 140, 2)
reject(lambda: dds_fingerprint(dx10bad), "array texture")

name=b"test_s\0"
data_start=0x24+len(name)
tpf=bytearray(data_start+len(bc1))
tpf[:4]=b"TPF\0"
struct.pack_into("<III",tpf,4,len(bc1),1,0x20300)
struct.pack_into("<5I",tpf,16,data_start,len(bc1),0,0x24,0)
tpf[0x24:data_start]=name
tpf[data_start:]=bc1
state,entries=parse_tpf(bytes(tpf),"synthetic")
assert state=="nonempty" and len(entries)==1
assert entries[0]["tpf_name"]=="test_s"
assert entries[0]["gpu_mip_bytes_sha256"]==identity["gpu_mip_bytes_sha256"]
tpf_bad=bytearray(tpf)
struct.pack_into("<I",tpf_bad,4,len(bc1)-1)
reject(lambda: parse_tpf(tpf_bad,"synthetic"), "TPF data size")
reject(lambda: parse_tpf(tpf+b"TPF\0","synthetic"), "duplicate TPF")
assert parse_tpf(b"no tpf","synthetic")[0]=="absent"
empty=b"TPF\0"+struct.pack("<III",0,0,0x20300)
assert parse_tpf(empty,"synthetic")[0]=="empty"
print("PASS: exact DDS BC1/BC5 full-mip fingerprints and strict TPF structural rejections")
