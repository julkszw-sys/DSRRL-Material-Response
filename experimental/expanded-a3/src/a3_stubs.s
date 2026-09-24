.intel_syntax noprefix
.text
.global a3_gate_stub
.global a3_pre_stub
.global a3_draw_indexed_stub
.global a3_draw_instanced_stub
.global a3_selector_hook_entry
.extern a3_capture_receiver
.extern a3_capture_pre_ps
.extern a3_call_legacy_gate
.extern a3_call_legacy_pre
.extern a3_prepare_draw
.extern a3_finish_draw
.extern a3_selector_observer
.extern g_a3_selector_trampoline

# Replaces call 0x18019c0e1. Preserve rcx/rdx and forward original return AL.
a3_gate_stub:
  sub rsp, 0x38
  mov [rsp+0x20], rcx
  mov [rsp+0x28], rdx
  mov rcx, rdx
  call a3_capture_receiver
  mov rcx, [rsp+0x20]
  mov rdx, [rsp+0x28]
  call a3_call_legacy_gate
  add rsp, 0x38
  ret

# Replaces call 0x18019c068. That helper uses RBX as native context.
a3_pre_stub:
  sub rsp, 0x28
  call a3_call_legacy_pre
  # legacy_pre returns a live pointer in RAX; shipping 1.45 consumes it
  # immediately after this callsite. Preserve it across A3 telemetry/capture.
  mov [rsp+0x20], rax
  mov rcx, rbx
  call a3_capture_pre_ps
  mov rax, [rsp+0x20]
  add rsp, 0x28
  ret

# Original target pointer is already in RAX. Arguments: rcx, edx, r8d, r9d.
a3_draw_indexed_stub:
  sub rsp, 0x58
  mov [rsp+0x20], rax
  mov [rsp+0x28], rcx
  mov [rsp+0x30], rdx
  mov [rsp+0x38], r8
  mov [rsp+0x40], r9
  call a3_prepare_draw
  mov rcx, [rsp+0x28]
  mov rdx, [rsp+0x30]
  mov r8,  [rsp+0x38]
  mov r9,  [rsp+0x40]
  mov rax, [rsp+0x20]
  call rax
  mov rcx, [rsp+0x28]
  call a3_finish_draw
  add rsp, 0x58
  ret

# Entry stack: ret, shadow[32], arg5 at +0x28, arg6 at +0x30.
a3_draw_instanced_stub:
  mov r10d, [rsp+0x28]
  mov r11d, [rsp+0x30]
  sub rsp, 0x68
  mov [rsp+0x30], rax
  mov [rsp+0x38], rcx
  mov [rsp+0x40], rdx
  mov [rsp+0x48], r8
  mov [rsp+0x50], r9
  mov [rsp+0x58], r10d
  mov [rsp+0x5c], r11d
  call a3_prepare_draw
  mov rcx, [rsp+0x38]
  mov rdx, [rsp+0x40]
  mov r8,  [rsp+0x48]
  mov r9,  [rsp+0x50]
  mov eax, [rsp+0x58]
  mov [rsp+0x20], eax
  mov eax, [rsp+0x5c]
  mov [rsp+0x28], eax
  mov rax, [rsp+0x30]
  call rax
  mov rcx, [rsp+0x38]
  call a3_finish_draw
  add rsp, 0x68
  ret

# Exact historical selector observer shim.
a3_selector_hook_entry:
  pushfq
  push rax
  push rcx
  push rdx
  push r8
  push r9
  push r10
  push r11
  sub rsp, 0x88
  movdqu [rsp+0x20], xmm0
  movdqu [rsp+0x30], xmm1
  movdqu [rsp+0x40], xmm2
  movdqu [rsp+0x50], xmm3
  movdqu [rsp+0x60], xmm4
  movdqu [rsp+0x70], xmm5
  mov rcx, [rsp+0xa8]
  mov rdx, [rsp+0xc8]
  mov r8, r14
  mov r9, r15
  call a3_selector_observer
  movdqu xmm0, [rsp+0x20]
  movdqu xmm1, [rsp+0x30]
  movdqu xmm2, [rsp+0x40]
  movdqu xmm3, [rsp+0x50]
  movdqu xmm4, [rsp+0x60]
  movdqu xmm5, [rsp+0x70]
  add rsp, 0x88
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdx
  pop rcx
  pop rax
  popfq
  mov rax, qword ptr [rip+g_a3_selector_trampoline]
  jmp rax