option casemap:none

EXTERN dsrrl_texture_name_observer:PROC
EXTERN dsrrl_texture_name_clear_observer:PROC
EXTERN g_dsrrl_texture_name_resume:QWORD
EXTERN g_dsrrl_texture_name_clear_resume:QWORD

.code

PUBLIC dsrrl_texture_name_hook_entry
dsrrl_texture_name_hook_entry PROC
    lea r14, [rbp-19h]
    cmp qword ptr [rbp-1], 8
    cmovae r14, qword ptr [rbp-19h]

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

    mov rcx, r14
    call dsrrl_texture_name_observer

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

    jmp qword ptr [g_dsrrl_texture_name_resume]
dsrrl_texture_name_hook_entry ENDP

PUBLIC dsrrl_texture_name_clear_hook_entry
dsrrl_texture_name_clear_hook_entry PROC
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

    call dsrrl_texture_name_clear_observer

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

    mov rbx, qword ptr [rsp+0F0h]
    add rsp, 0A0h

    jmp qword ptr [g_dsrrl_texture_name_clear_resume]
dsrrl_texture_name_clear_hook_entry ENDP

END
