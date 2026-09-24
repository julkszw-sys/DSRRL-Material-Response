option casemap:none

EXTERN texture_name_observer:PROC
EXTERN texture_name_clear_observer:PROC
EXTERN g_texture_name_resume:QWORD
EXTERN g_texture_name_clear_resume:QWORD

.code

PUBLIC texture_name_hook_entry
texture_name_hook_entry PROC
    ; Re-execute exact retail bytes from 0x140583AA6..0x140583AB3.
    lea r14, [rbp-19h]
    cmp qword ptr [rbp-1], 8
    cmovae r14, qword ptr [rbp-19h]

    ; At this mid-function site retail RSP is already 16-byte aligned.
    ; Eight pushes preserve that alignment; reserve A0h (not A8h) so the
    ; callback obeys the Windows x64 call-site alignment requirement.
    ; Preserve the architectural state that must reach 0x140583AB4.
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
    call texture_name_observer

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

    jmp qword ptr [g_texture_name_resume]
texture_name_hook_entry ENDP

PUBLIC texture_name_clear_hook_entry
texture_name_clear_hook_entry PROC
    ; Clear the exact logical-name scope at the certified 0x140583E81 boundary.
    ; This is also a mid-function body site with aligned retail RSP, so A0h
    ; keeps the observer call ABI-correct after eight pushes.
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

    call texture_name_clear_observer

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

    ; Re-execute exact retail bytes from 0x140583E81..0x140583E8F.
    mov rbx, qword ptr [rsp+0F0h]
    add rsp, 0A0h

    jmp qword ptr [g_texture_name_clear_resume]
texture_name_clear_hook_entry ENDP

END
