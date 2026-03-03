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

