.global _start

/*
 * Tiny homebrew C runtime
 * TODO: Handle argc, argv
 */

.section .text
_start:
    call   main

    mov    %rax, %rdi   // Arg 1: Return value of "main"
    mov    $60, %rax    // Syscall id (exit)
    syscall
