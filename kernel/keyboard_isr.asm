[BITS 32]
[EXTERN keyboard_handler]
[GLOBAL keyboard_isr]

keyboard_isr:
    pusha
    call keyboard_handler
    popa
    iret
