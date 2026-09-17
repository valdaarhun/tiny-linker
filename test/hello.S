.section .text
// Print "hello world"
hello:
    // Set up frame and stack
    push    %rbp
    mov     %rsp, %rbp

    // Write hello world to stdout
    mov     $1, %rdi         // Arg 1: fd = 1 (stdout)
    mov     $message, %rsi   // Arg 2: Address of "message"
    mov     $13, %rdx        // Arg 3: Length of "message"
    mov     $1, %rax         // Syscall id (write)
    syscall

    // Clean up frame and return
    pop     %rbp
    ret

.section .data
message:
    .ascii    "hello world\n\0"
