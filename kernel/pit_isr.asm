[BITS 32]
[EXTERN pit_handler]
[GLOBAL pit_isr]

pit_isr:
    pusha
    call pit_handler
    popa
    iret