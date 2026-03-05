[BITS 16]
[ORG 0x7C00]
jmp short start
nop
; === BPB (BIOS Parameter Block) — FAT12 header ===
db "MSDOS5.0"       ; OEM Name (8 байт)
dw 512              ; Bytes per sector
db 1                ; Sectors per cluster
dw 1                ; Reserved sectors
db 2                ; Number of FATs
dw 224              ; Root entry count
dw 2880             ; Total sectors
db 0xF0             ; Media type (floppy)
dw 9                ; Sectors per FAT
dw 18               ; Sectors per track
dw 2                ; Number of heads
dd 0                ; Hidden sectors
dd 0                ; Large sector count
start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov si, message
.loop:
    lodsb
    or al, al
    jz .load_kernel
    mov ah, 0x0E
    int 0x10
    jmp .loop
.load_kernel:
    mov ah, 0x02
    mov al, 20
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x00
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    int 0x13
    jc disk_error
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry
disk_error:
    mov si, error_msg
.err_loop:
    lodsb
    or al, al
    jz .freeze
    mov ah, 0x0E
    int 0x10
    jmp .err_loop
.freeze:
    cli
    hlt
    jmp .freeze
gdt_start:
    dq 0
gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
[BITS 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov esp, 0x90000
    jmp 0x10000
message   db 'loading kernel... wait a sec...', 13, 10, 0
error_msg db 'disk error!', 13, 10, 0
times 510-($-$$) db 0
dw 0xAA55
