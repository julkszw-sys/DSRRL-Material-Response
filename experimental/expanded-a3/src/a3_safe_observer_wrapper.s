.intel_syntax noprefix
.text
.globl a3_safe_observer_wrapper
a3_safe_observer_wrapper:
  sub rsp, 0x58
  mov qword ptr [rsp+0x30], rcx
  mov qword ptr [rsp+0x38], rdx
  mov qword ptr [rsp+0x40], r8
  mov qword ptr [rsp+0x48], r9

  # Patched by hotfix_safe_observer3.py:
  # shipping 1.45 EXE-base global at addon RVA 0x1076B8.
  .byte 0x48,0x8b,0x05
  .long 0

  mov r10, rdx
  sub r10, rax
  cmp r10, 0x20e019
  je .use_r9
  cmp r10, 0x20fb9e
  je .use_r9
  cmp r10, 0x20eb7f
  je .use_r8
  jmp .done

.use_r9:
  mov r11, r9
  jmp .validate
.use_r8:
  mov r11, r8

.validate:
  test rcx, rcx
  je .done
  test r11, r11
  je .done
  lea rcx, [r11+0x4c]
  lea rdx, [rsp+0x20]

  # Patched by hotfix_safe_observer3.py:
  # call shipping 1.45 safe-read8 helper at addon RVA 0x2A60.
  .byte 0xe8
  .long 0

  test al, al
  je .done

  mov rcx, qword ptr [rsp+0x30]
  mov rdx, qword ptr [rsp+0x38]
  mov r8,  qword ptr [rsp+0x40]
  mov r9,  qword ptr [rsp+0x48]

  # Patched by hotfix_safe_observer3.py:
  # call existing A3 selector observer.
  .byte 0xe8
  .long 0

.done:
  add rsp, 0x58
  ret
