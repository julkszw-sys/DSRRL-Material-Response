option casemap:none

EXTERN dsrrl_flver_selector_observer:PROC
EXTERN g_dsrrl_flver_selector_trampoline:QWORD

.code

PUBLIC dsrrl_flver_selector_hook_entry
dsrrl_flver_selector_hook_entry PROC
    ; Function-entry hook at 0x14022BA20. Entry RSP is 8 mod 16.
    ; Reserve Win64 shadow space, three stack arguments and four saved ABI
    ; register arguments. 68h changes alignment by 8, so RSP is 16-byte
    ; aligned at CALL.
    sub rsp,068h

    mov qword ptr [rsp+38h],rcx
    mov qword ptr [rsp+40h],rdx
    mov qword ptr [rsp+48h],r8
    mov qword ptr [rsp+50h],r9

    ; dsrrl_flver_selector_observer(
    ;   container=RCX, owner=RDX, ret=[entry RSP],
    ;   r14, r15, material_index=R8D, incoming_mode=R9D)
    mov qword ptr [rsp+20h],r15
    mov qword ptr [rsp+28h],r8
    mov qword ptr [rsp+30h],r9
    mov r8,qword ptr [rsp+68h]
    mov r9,r14
    call dsrrl_flver_selector_observer

    mov rcx,qword ptr [rsp+38h]
    mov rdx,qword ptr [rsp+40h]
    mov r8,qword ptr [rsp+48h]
    mov r9,qword ptr [rsp+50h]
    add rsp,068h

    jmp qword ptr [g_dsrrl_flver_selector_trampoline]
dsrrl_flver_selector_hook_entry ENDP

END
