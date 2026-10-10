# SPC25 offline — original DSR TPF content identity and CPU payload boundary

Exact host SHA256: a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b.
Pinned owner source ZIP SHA256: a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97.

## FrpgTextureEntity native boundary

CPU virtual methods 0x14057EEB0 and 0x14057EE70 use field +0x18 when set; otherwise query the fallback object manager 0x1411B3D50 and dispatch through its virtual slots +0x40/+0x48. The decoder 0x14057F000 drops its previous +0x18 pointer by decrementing an intrusive reference counter at managed payload+0x08 (via 0x140CC4030, lock xadd). Last-reference destruction calls payload's first virtual function. This is an engine-managed object and not a directly proven native D3D11 COM resource or shader-resource view. Direct casting to an SRV is prohibited. Four additional byte anchors in the read-only verifier pin these observations (43 total).

## Exact DSR source texture fingerprints

Owner parts BND evidence: 111 archives; 31 without TPF, 37 empty TPF, 43 nonempty TPF, 85 exact DDS entries with 70 unique logical names. DDS format counts: 50 BC1/DXT1, 2 BC3/DXT5, 33 BC5/DX10 format83. All 85 compressed mip payload sizes verify against DDS width, height, mips and block layout. No repeated logical name maps to conflicting full compressed payload SHA256 values in this corpus.

The exact same-archive FLVER material slot to logical g_Specular name to DDS blob chain is proven for 13 source material slots of the ONE profile P_Metal[DSB].mtd. Full compressed mip bytes are separately hashed, not only DDS header. This is SOURCE identity only: no evidence that a given live D3D11 resource or stock PS t1 view has those bytes.

Offline tool: renderer-core/tools/verify_spc25_dsr_tpf_dds_fingerprints_offline.py. Separate existing FLVER-only source map covers 12/25 profiles, whereas original DDS bytes cover only this P_Metal source subset; never conflate the scopes.

## Bridge proof obligations

Only a future exact native D3D11 texture match across every original compressed mip, with correct format, dimensions, pitches, device/view lifetime and conflict quarantine, could tie a stock resource to this manifest. Then verify real bound PS t1 and authenticated FLVER/slot/raw MTD/receiver. Partial uploads, renamed textures, texture format changes or missing initial data must fail open. No blanket LightBank or PBL rollback. No runtime test or PTDE pixel proof. Existing addon/V13 untouched.
