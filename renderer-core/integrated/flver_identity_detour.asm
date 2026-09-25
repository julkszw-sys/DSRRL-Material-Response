option casemap:none

EXTERN dsrrl_flver_selector_observer:PROC
EXTERN g_dsrrl_flver_selector_trampoline:QWORD

.code

PUBLIC dsrrl_flver_selector_hook_entry
dsrrl_flver_selector_hook_entry PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp,0A8h

    ; 20h..2Fh are arg5/arg6; 30h is arg7. Keep XMM saves above it.
    movdqu xmmword ptr [rsp+40h],xmm0
    movdqu xmmword ptr [rsp+50h],xmm1
    movdqu xmmword ptr [rsp+60h],xmm2
    movdqu xmmword ptr [rsp+70h],xmm3
    movdqu xmmword ptr [rsp+80h],xmm4
    movdqu xmmword ptr [rsp+90h],xmm5

    ; Original selector ABI at 0x14022BA20:
    ; RCX = material container
    ; RDX = LightBank/render owner
    ; R8D = material index
    ; R9D = incoming lighting semantic mode
    ; live R14/R15 carry the assignment descriptor depending on caller.
    mov rcx,qword ptr [rsp+0D0h]
    mov rdx,qword ptr [rsp+0C8h]
    mov r8,qword ptr [rsp+0E8h]
    mov r9,r14
    mov qword ptr [rsp+20h],r15
    mov rax,qword ptr [rsp+0C0h]
    mov qword ptr [rsp+28h],rax
    mov rax,qword ptr [rsp+0B8h]
    mov qword ptr [rsp+30h],rax
    call dsrrl_flver_selector_observer

    movdqu xmm0,xmmword ptr [rsp+40h]
    movdqu xmm1,xmmword ptr [rsp+50h]
    movdqu xmm2,xmmword ptr [rsp+60h]
    movdqu xmm3,xmmword ptr [rsp+70h]
    movdqu xmm4,xmmword ptr [rsp+80h]
    movdqu xmm5,xmmword ptr [rsp+90h]

    add rsp,0A8h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    jmp qword ptr [g_dsrrl_flver_selector_trampoline]
dsrrl_flver_selector_hook_entry ENDP

END
