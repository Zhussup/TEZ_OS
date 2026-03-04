[BITS 16]        ; 16-битный реальный режим
[ORG 0x7C00]     ; BIOS загружает нас сюда

start:
    ; Настраиваем сегментные регистры
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00


    ; Выводим строку через BIOS
    mov si, message

.loop:
    lodsb            ; Загружаем следующий байт строки в AL
    or al, al        ; Проверяем на ноль (конец строки)
    jz .load_kernel
    mov ah, 0x0E     ; Функция BIOS: вывод символа
    int 0x10         ; Вызов BIOS
    jmp .loop

.load_kernel:
    mov ah, 0x02
    mov al, 20
	mov ch, 0
	mov cl, 2
	mov dh, 0
	mov bx, 0x1000
	mov es, bx
	xor bx, bx
	int 0x13
	jc disk_error

	cli
	lgdt [gdt_descriptor]
	mov eac, cr0
	or eax, 1
	mov cr0, eax
	jmp 0x08:pm_entry

disk_error: 
    mov si, error_msg

.err_loop:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .err_loop

.hang:
    hlt


; GDT! GDT! GDT!

gdt_start:
	dq 0
gdt_code:
	dw 0xFFFFm 0x0000
	db 0x00, 0x9A, 0xCF, 0x00
gdt_data:
	dw 0xFFFF, 0x0000
	db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_descriptor:
	dw gdt_end - gdt_end - 1
	dd gdt_start

; 32Bit code

[BITS 32]

pm_entry:
	mov ax, 0x10 ;data segment
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov esp, 0x90000 ;stack

	;jmp yo kernrl addressed 

	;we're loading in 0x10000
	jump 0x10000

message db 'loading kernel... wait a sec...', 13, 10, 0
error_msg db 'disk error!', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
