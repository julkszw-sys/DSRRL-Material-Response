option casemap:none

EXTERN dsrrl_hemdir3_effective_mode_observer:PROC
EXTERN dsrrl_hemdir3_selector_end_observer:PROC
EXTERN g_dsrrl_hemdir3_mode_lt5_trampoline:QWORD
EXTERN g_dsrrl_hemdir3_selector_end_trampoline:QWORD

.code

; Exact site 0x140295F9F. At this point EDX is the post-override effective
; semantic mode. The remaining stock <5 path needs only RDX/R9/R10/R11 from
; volatile state; RAX/RCX are overwritten by the stolen instructions and R8
; is dead. No XMM register is live across this integer-only lookup.
PUBLIC dsrrl_hemdir3_mode_lt5_hook_entry
dsrrl_hemdir3_mode_lt5_hook_entry PROC
    ; Current RSP is 8 mod 16. 48h makes it 16-byte aligned at CALL and
    ; provides shadow space plus four saved live registers.
    sub rsp,048h
    mov qword ptr [rsp+20h],rdx
    mov qword ptr [rsp+28h],r9
    mov qword ptr [rsp+30h],r10
    mov qword ptr [rsp+38h],r11

    mov ecx,edx
    call dsrrl_hemdir3_effective_mode_observer

    mov rdx,qword ptr [rsp+20h]
    mov r9,qword ptr [rsp+28h]
    mov r10,qword ptr [rsp+30h]
    mov r11,qword ptr [rsp+38h]
    add rsp,048h

    jmp qword ptr [g_dsrrl_hemdir3_mode_lt5_trampoline]
dsrrl_hemdir3_mode_lt5_hook_entry ENDP

; Exact site 0x14022BA79 after CALL 0x140295F50. The continuation immediately
; overwrites R8/RDX/RCX and only needs EAX (lookup result) plus nonvolatile RBX.
; Preserve just RAX across the observer call.
PUBLIC dsrrl_hemdir3_selector_end_hook_entry
dsrrl_hemdir3_selector_end_hook_entry PROC
    ; Parent selector local RSP is 16-byte aligned here.
    sub rsp,030h
    mov qword ptr [rsp+20h],rax
    call dsrrl_hemdir3_selector_end_observer
    mov rax,qword ptr [rsp+20h]
    add rsp,030h

    jmp qword ptr [g_dsrrl_hemdir3_selector_end_trampoline]
dsrrl_hemdir3_selector_end_hook_entry ENDP

END
