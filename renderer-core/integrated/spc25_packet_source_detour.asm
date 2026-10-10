option casemap:none

; SPC25 experimental CPU producer cut. Only installed after exact host SHA and
; byte-for-byte site certification. The original retail path is:
; mov rcx,rax / mov r8,rdi / mov edx,esi / call 0x14057EFB0 / jmp target.
; RBX is the named cache entry, RDI is the *engine* resource object, not SRV.
; This probe observes, then calls the original writer and resumes at its
; original jump destination. No GPU or disk mutation, no draw replay.
EXTERN dsrrl_spc25_packet_source_observer:PROC
EXTERN g_dsrrl_spc25_writer_target:QWORD
EXTERN g_dsrrl_spc25_packet_resume:QWORD

.code
PUBLIC dsrrl_spc25_packet_source_hook_entry
dsrrl_spc25_packet_source_hook_entry PROC
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
    call dsrrl_spc25_packet_source_observer

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
    call qword ptr [g_dsrrl_spc25_writer_target]
    jmp qword ptr [g_dsrrl_spc25_packet_resume]
dsrrl_spc25_packet_source_hook_entry ENDP
END
