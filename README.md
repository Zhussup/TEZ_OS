# TEZ_OS

A simple 32-bit operating system written from scratch in C and Assembly.

## Features
- Custom bootloader (Real Mode → Protected Mode)
- FAT12 filesystem support
- VGA text mode output
- PS/2 keyboard driver (IRQ1, scan codes, Shift, Caps Lock)
- IDT & PIC setup

## Project Structure
```
TEZ_OS/
├── boot/
│   └── boot.asm        # Bootloader (16-bit, loads kernel)
├── kernel/
│   ├── entry.asm       # Kernel entry point (_start)
│   ├── kernel.c        # Main kernel, VGA driver
│   ├── idt.c           # IDT, PIC setup
│   ├── keyboard.c      # PS/2 keyboard driver
│   ├── keyboard_isr.asm# IRQ1 interrupt stub
│   ├── ata.c           # ATA PIO disk driver
│   ├── fat12.c         # FAT12 filesystem
│   └── linker.ld       # Linker script
├── Makefile
├── .gitignore
└── README.md
```

## Requirements
- `nasm`
- `gcc` with `-m32` support (`gcc-multilib`)
- `binutils` (`ld`, `objcopy`)
- `make`
- `qemu-system-i386`
- `mtools` (optional, for copying files to disk image)

### Install (Ubuntu/Debian)
```bash
sudo apt install nasm gcc gcc-multilib binutils make qemu-system-x86 mtools
```

## Build & Run
```bash
make clean && make
qemu-system-i386 -drive file=os.img,format=raw,if=floppy
```

## Add a file to disk
```bash
echo "Hello from TEZ_OS!" > hello.txt
mcopy -i os.img hello.txt ::HELLO.TXT
```
