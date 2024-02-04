.intel_syntax noprefix
.extern printf

.data
msg: .ascii "\nrax: 0x%.16lx, rbx: 0x%.16lx, rcx: 0x%.16lx\n"

.text
.global attackable
attackable:
    pop rcx
    xor rcx, rcx
    jmp rcx
    ret
    pop rdx
    pop rax
    jmp rax
    xor rcx, rcx
    mov rbx, rcx
    call syscall_equivalent
    ret
    pop rax
    ret
    pop rcx
    xor rcx, rcx
    ret
    mov rcx, rdx
    mov ebx, 0x1
    ret
    pop rbx
    xor rax, rbx
    ret
    
.global syscall_equivalent
syscall_equivalent:
    and rsp, -16
    mov rbp, rsp
    mov rsi, rax
    mov rdx, rbx
    lea rdi, [rip + msg]
    call printf
    mov rax, 60
    mov rdi, 0
    syscall
    ret

