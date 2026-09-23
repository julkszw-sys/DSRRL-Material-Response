.intel_syntax noprefix
.text
.globl integrated_pre_stub
.globl integrated_post_stub
.globl capture_gate_stub
.globl path_gate_stub
.globl integrated_gate_stub
.globl integrated_selector_stub

integrated_pre_stub:
    sub rsp, 0x28
    mov rcx, rbx
    call asset_pre
    add rsp, 0x28
    .byte 0xE9
pre_chain_rel32:
    .long 0x51515151

integrated_post_stub:
    sub rsp, 0x28
    mov rcx, rbx
    call asset_post
    add rsp, 0x28
    .byte 0xE9
post_chain_rel32:
    .long 0x52525252

# RBX=name UTF-16, R14D=len. Accept SpecRGB, Normal and whitelisted Diffuse identities.
capture_gate_stub:
    sub rsp, 0x20
    mov rcx, rbx
    mov edx, r14d
    call should_track_name
    add rsp, 0x20
    test eax, eax
    jz capture_reject
    .byte 0xE9
capture_accept_rel32:
    .long 0x53535353
capture_reject:
    .byte 0xE9
capture_reject_rel32:
    .long 0x54545454

# R12=SAFE state, R14=exact-name record, scratch path original RSP+0x1c0.
# build_asset_path uses the synchronous per-context loading marker to select Normals vs Diffuse.
path_gate_stub:
    lea r8, [rsp + 0x1c0]
    mov rcx, r12
    mov rdx, r14
    sub rsp, 0x20
    call build_asset_path
    add rsp, 0x20
    test eax, eax
    jz path_original
    .byte 0xE9
path_asset_rel32:
    .long 0x55555555
path_original:
    mov edx, dword ptr [r12 + 0x34]
    .byte 0xE9
path_orig_rel32:
    .long 0x56565656

# Replacement for draw gate call at 0x632e.
# First preserve exact build52 Subsurf/shared gate semantics, then opportunistically prepare Diffuse.
integrated_gate_stub:
    push rcx
    push rdx
    sub rsp, 0x28
    .byte 0xE8
gate_subsurf_rel32:
    .long 0x57575757
    test al, al
    jz gate_done
    mov rcx, qword ptr [rsp + 0x30]
    mov edx, edi
    call prepare_diffuse
    mov al, 1
gate_done:
    add rsp, 0x28
    pop rdx
    pop rcx
    ret

# Replacement selector target at 0x63fc.
# If route flag2 has a verified PTDE diffuse companion, convert this draw only to flag1
# so existing full c100+c101 diffuse-linear receiver semantics are selected.
integrated_selector_stub:
    cmp r12b, 2
    jne selector_chain
    sub rsp, 0x20
    mov rcx, rbx
    call diffuse_ready
    add rsp, 0x20
    test eax, eax
    jz selector_chain
    mov r12b, 1
selector_chain:
    .byte 0xE9
selector_subsurf_rel32:
    .long 0x58585858
