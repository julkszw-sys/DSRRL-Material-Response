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
 movdqu xmmword ptr [rsp+30h],xmm0
 movdqu xmmword ptr [rsp+40h],xmm1
 movdqu xmmword ptr [rsp+50h],xmm2
 movdqu xmmword ptr [rsp+60h],xmm3
 movdqu xmmword ptr [rsp+70h],xmm4
 movdqu xmmword ptr [rsp+80h],xmm5
 mov rcx,qword ptr [rsp+0D0h]
 mov rdx,qword ptr [rsp+0C0h]
 mov r8,qword ptr [rsp+0E8h]
 call dsrrl_flver_selector_observer
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
 jmp qword ptr [g_dsrrl_flver_selector_trampoline]
dsrrl_flver_selector_hook_entry ENDP
END
