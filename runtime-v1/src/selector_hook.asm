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
    sub rsp, 0A8h

    ; 20h..2Fh reserved for Windows x64 5th argument / shadow-space tail.
    movdqu xmmword ptr [rsp+30h], xmm0
    movdqu xmmword ptr [rsp+40h], xmm1
    movdqu xmmword ptr [rsp+50h], xmm2
    movdqu xmmword ptr [rsp+60h], xmm3
    movdqu xmmword ptr [rsp+70h], xmm4
    movdqu xmmword ptr [rsp+80h], xmm5

    ; Original stack S0 is rsp+0E8h after eight pushes and sub 0A8h.
    ; Saved original RCX(material container)=rsp+0D0h, RDX(owner)=rsp+0C8h, R8(material index)=rsp+0C0h.
    ; Retail 0x14022BA20 resolves actual parsed material as *([RCX+10h] + 24*R8D).
    mov rcx, qword ptr [rsp+0D0h]
    mov rdx, qword ptr [rsp+0C8h]
    mov r8,  qword ptr [rsp+0E8h]
    mov r9,  r14
    mov qword ptr [rsp+20h], r15
    mov rax, qword ptr [rsp+0C0h]
    mov qword ptr [rsp+28h], rax
    call selector_observer

    movdqu xmm0, xmmword ptr [rsp+30h]
    movdqu xmm1, xmmword ptr [rsp+40h]
    movdqu xmm2, xmmword ptr [rsp+50h]
    movdqu xmm3, xmmword ptr [rsp+60h]
    movdqu xmm4, xmmword ptr [rsp+70h]
    movdqu xmm5, xmmword ptr [rsp+80h]

    add rsp, 0A8h
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