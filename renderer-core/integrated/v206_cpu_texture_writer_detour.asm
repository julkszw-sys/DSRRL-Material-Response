option casemap:none
; V206 narrow adapter for the previously verified retail CPU writer 0x583BCE.
; Preserves the retail call sequence and all volatile GP/XMM registers.
; Unlike legacy SPC25 there is NO typed decoder and NO NPC c5330 handler.
EXTERN dsrrl_v206_writer_observer:PROC
EXTERN g_dsrrl_v206_writer_target:QWORD
EXTERN g_dsrrl_v206_writer_resume:QWORD

.code
PUBLIC dsrrl_v206_writer_hook_entry
dsrrl_v206_writer_hook_entry PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, 0A0h
    movdqu xmmword ptr [rsp+30h], xmm0
    movdqu xmmword ptr [rsp+40h], xmm1
    movdqu xmmword ptr [rsp+50h], xmm2
    movdqu xmmword ptr [rsp+60h], xmm3
    movdqu xmmword ptr [rsp+70h], xmm4
    movdqu xmmword ptr [rsp+80h], xmm5
    mov rcx, rbx
    mov rdx, rdi
    mov r8d, esi
    call dsrrl_v206_writer_observer
    movdqu xmm0, xmmword ptr [rsp+30h]
    movdqu xmm1, xmmword ptr [rsp+40h]
    movdqu xmm2, xmmword ptr [rsp+50h]
    movdqu xmm3, xmmword ptr [rsp+60h]
    movdqu xmm4, xmmword ptr [rsp+70h]
    movdqu xmm5, xmmword ptr [rsp+80h]
    add rsp, 0A0h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    mov rcx, rax
    mov r8, rdi
    mov edx, esi
    call qword ptr [g_dsrrl_v206_writer_target]
    jmp qword ptr [g_dsrrl_v206_writer_resume]
dsrrl_v206_writer_hook_entry ENDP
END
