import json, re, pathlib, hashlib, collections
root=pathlib.Path('/mnt/data/dsrrl_closed_build')
plan_path=root/'p22/DSRRL_RENDERER_P2_2_HDR_OFF_CAUSAL_CONTROL_2026-09-07/data/PORT_PLAN.json'
release_path=root/'release/DSRRL_Material_Response_1.45.addon64'
d=json.load(open(plan_path,'r',encoding='utf-8'))
release=release_path.read_bytes()
release_names=sorted(set(x.decode('ascii') for x in re.findall(rb'FRPG_[A-Za-z0-9_]+\.fpo',release)))
release_set=set(release_names)
ALLOWED=1|2|4|8|32|64|128|256
selected=[]; skipped_overlap=[]; skipped_status=[]
for p in d['plans']:
    mask=int(p['mask'])
    names={a['name'] for a in p.get('aliases',[])}
    if mask==0 or (mask & ~ALLOWED):
        skipped_status.append(p); continue
    if p.get('broad_alias') or p.get('collateral_count') or any((int(a.get('desired_mask',0)) & mask) != mask for a in p.get('aliases',[])):
        skipped_status.append(p); continue
    if names & release_set:
        skipped_overlap.append(p); continue
    selected.append(p)
selected.sort(key=lambda p:(p['code_size'],p['original_sha256']))
embedded=[]
for i in range(len(release)-32):
    if release[i:i+4]!=b'DXBC': continue
    if i+28>len(release): continue
    total=int.from_bytes(release[i+24:i+28],'little')
    if total<32 or i+total>len(release): continue
    blob=release[i:i+total]
    embedded.append(hashlib.sha256(blob).hexdigest())
embedded_set=set(embedded)
orig_inter=[p['original_sha256'] for p in selected if p['original_sha256'] in embedded_set]
repl_inter=[p['replacement_sha256'] for p in selected if p['replacement_sha256'] in embedded_set]
h=root/'src/closed_plan.hpp'
with h.open('w',encoding='utf-8') as f:
    f.write('#pragma once\n')
    f.write('typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long long u64;\n')
    f.write('struct PatchOp { u32 off; u32 oldw; u32 neww; };\n')
    f.write('struct Plan { u8 sha[32]; u8 repl_sha[32]; u32 size; u16 mask; u8 op_count; u8 reserved; PatchOp ops[12]; };\n')
    f.write(f'static const u16 kAllowedMask = {ALLOWED}u;\n')
    f.write(f'static const u32 kPlanCount = {len(selected)}u;\n')
    f.write('static const Plan kPlans[] = {\n')
    for p in selected:
        sha=bytes.fromhex(p['original_sha256']); rh=bytes.fromhex(p['replacement_sha256'])
        sb=','.join(f'0x{x:02x}' for x in sha); rb=','.join(f'0x{x:02x}' for x in rh)
        ops=list(p['ops'])
        ops_s=[]
        for op in ops:
            ops_s.append('{%du,0x%08xu,0x%08xu}'%(int(op['byte_offset']),int(op['old']),int(op['new'])))
        while len(ops_s)<12: ops_s.append('{0u,0u,0u}')
        f.write('  { {'+sb+'}, {'+rb+'}, %du, %du, %du, 0u, { %s } },\n'%(p['code_size'],p['mask'],len(ops),','.join(ops_s)))
    f.write('};\n')
reason_counts=collections.Counter()
mask_counts=collections.Counter()
aliases=set()
for p in selected:
    mask_counts[str(p['mask'])]+=1
    aliases.update(a['name'] for a in p.get('aliases',[]))
    for op in p['ops']: reason_counts[op['reason']]+=1
overlap_aliases=sorted({a['name'] for p in skipped_overlap for a in p.get('aliases',[]) if a['name'] in release_set})
audit={
 'schema':'DSRRL_CLOSED_OPERATORS_1_45_BUILD_AUDIT_V1',
 'release':{'file':release_path.name,'size':len(release),'sha256':hashlib.sha256(release).hexdigest(),'embedded_dxbc_count':len(embedded),'embedded_dxbc_unique':len(embedded_set),'embedded_shader_name_count':len(release_names)},
 'p22_plan_source':{'file':'PORT_PLAN.json','source_binder_sha256':d['source_binder_sha256'],'plan_hashes_total':len(d['plans'])},
 'selection':{'allowed_mask':ALLOWED,'allowed_bits':[1,2,4,8,32,64,128,256],'selected_plan_hashes':len(selected),'selected_alias_union':len(aliases),'selected_ops':sum(len(p['ops']) for p in selected),'skipped_nonclosed_or_mixed':len(skipped_status),'skipped_release_overlap_plans':len(skipped_overlap),'release_overlap_shader_names':overlap_aliases,'selected_original_hashes_in_release_embedded_dxbc':orig_inter,'selected_replacement_hashes_in_release_embedded_dxbc':repl_inter,'max_shader_size':max(p['code_size'] for p in selected),'mask_counts':dict(mask_counts),'op_reason_counts':dict(reason_counts)},
 'policy':{'composition':'exact released 1.45 unchanged + disjoint create_pipeline companion','release_receiver_overlap_policy':'skip entire P2.2 plan if any alias name occurs in shipping 1.45 binary','partial_hypothesis_policy':'excluded','broad_or_collateral_hash_policy':'excluded unless every alias requests the full selected mask; current build accepts no broad/collateral plan','runtime_fail_open':['unknown hash','code-size mismatch','old-word mismatch','DXBC invalid','replacement SHA mismatch'],'runtime_status':'NOT_TESTED','bridge_activation':'NOT_TESTED','pixel_behavior':'OPEN'}
}
json.dump(audit,open(root/'out/BUILD_AUDIT.precompile.json','w'),indent=2)
(root/'out/RELEASE_RECEIVER_NAMES.txt').write_text('\n'.join(release_names)+'\n')
print(json.dumps(audit['selection'],indent=2))
