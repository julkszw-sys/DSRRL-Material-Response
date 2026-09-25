option casemap:none

EXTERN dsrrl_hemdir3_effective_mode_observer:PROC
EXTERN g_dsrrl_hemdir3_mode_lt5_trampoline:QWORD
EXTERN g_dsrrl_hemdir3_mode_ge5_trampoline:QWORD

.code

PUBLIC dsrrl_hemdir3_mode_lt5_hook_entry
dsrrl_hemdir3_mode_lt5_hook_entry PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp,0A8h

    movdqu xmmword ptr [rsp+30h],xmm0
    movdqu xmmword ptr [rsp+40h],xmm1
    movdqu xmmword ptr [rsp+50h],xmm2
    movdqu xmmword ptr [rsp+60h],xmm3
    movdqu xmmword ptr [rsp+70h],xmm4
    movdqu xmmword ptr [rsp+80h],xmm5

    mov ecx,edx
    call dsrrl_hemdir3_effective_mode_observer

    movdqu xmm0,xmmword ptr [rsp+30h]
    movdqu xmm1,xmmword ptr [rsp+40h]
    movdqu xmm2,xmmword ptr [rsp+50h]
    movdqu xmm3,xmmword ptr [rsp+60h]
    movdqu xmm4,xmmword ptr [rsp+70h]
    movdqu xmm5,xmmword ptr [rsp+80h]

    add rsp,0A8h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    jmp qword ptr [g_dsrrl_hemdir3_mode_lt5_trampoline]
dsrrl_hemdir3_mode_lt5_hook_entry ENDP

PUBLIC dsrrl_hemdir3_mode_ge5_hook_entry
dsrrl_hemdir3_mode_ge5_hook_entry PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp,0A8h

    movdqu xmmword ptr [rsp+30h],xmm0
    movdqu xmmword ptr [rsp+40h],xmm1
    movdqu xmmword ptr [rsp+50h],xmm2
    movdqu xmmword ptr [rsp+60h],xmm3
    movdqu xmmword ptr [rsp+70h],xmm4
    movdqu xmmword ptr [rsp+80h],xmm5

    mov ecx,edx
    call dsrrl_hemdir3_effective_mode_observer

    movdqu xmm0,xmmword ptr [rsp+30h]
    movdqu xmm1,xmmword ptr [rsp+40h]
    movdqu xmm2,xmmword ptr [rsp+50h]
    movdqu xmm3,xmmword ptr [rsp+60h]
    movdqu xmm4,xmmword ptr [rsp+70h]
    movdqu xmm5,xmmword ptr [rsp+80h]

    add rsp,0A8h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    jmp qword ptr [g_dsrrl_hemdir3_mode_ge5_trampoline]
dsrrl_hemdir3_mode_ge5_hook_entry ENDP

END
