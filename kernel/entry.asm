[BITS 32]
[EXTERN kernel_main]

global _start

_start:

	mov esp, 0x90000

	call kernel_main

.hang:
	cli
	hlt
	jmp .hang

