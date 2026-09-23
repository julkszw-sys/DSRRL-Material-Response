option casemap:none

EXTERN selector_observer:PROC
EXTERN g_selector_trampoline:QWORD

.code
PUBLIC selector_hook_entry
selector_hook_entry PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, 88h

    movdqu xmmword ptr [rsp+20h], xmm0
    movdqu xmmword ptr [rsp+30h], xmm1
    movdqu xmmword ptr [rsp+40h], xmm2
    movdqu xmmword ptr [rsp+50h], xmm3
    movdqu xmmword ptr [rsp+60h], xmm4
    movdqu xmmword ptr [rsp+70h], xmm5

    ; Original stack S0 is rsp+0C8h after eight pushes and sub 88h.
    ; Original RDX (owner_context) is saved at rsp+0A8h.
    mov rcx, qword ptr [rsp+0A8h]
    mov rdx, qword ptr [rsp+0C8h]
    mov r8, r14
    mov r9, r15
    call selector_observer

    movdqu xmm0, xmmword ptr [rsp+20h]
    movdqu xmm1, xmmword ptr [rsp+30h]
    movdqu xmm2, xmmword ptr [rsp+40h]
    movdqu xmm3, xmmword ptr [rsp+50h]
    movdqu xmm4, xmmword ptr [rsp+60h]
    movdqu xmm5, xmmword ptr [rsp+70h]

    add rsp, 88h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    jmp qword ptr [g_selector_trampoline]
selector_hook_entry ENDP
END
