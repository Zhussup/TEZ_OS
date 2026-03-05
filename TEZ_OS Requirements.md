# TEZ_OS Requirements

## Build Tools
- `nasm` — assembler (boot.asm, entry.asm, keyboard_isr.asm)
- `gcc` — C compiler with `-m32` support (32-bit cross-compilation)
- `ld` — linker (from binutils)
- `objcopy` — ELF to binary conversion (from binutils)
- `make` — build system
- `dd` — disk image creation
- `python3` — writing FAT bytes to disk image

## Emulator
- `qemu-system-i386` — running the disk image

## Optional (for FAT12 file operations)
- `mtools` — `mcopy`, `mdir` for copying files into the disk image

## Install (Ubuntu/Debian)
```bash
sudo apt install nasm gcc gcc-multilib binutils make qemu-system-x86 mtools python3
```

## Build & Run
```bash
make clean && make
qemu-system-i386 -drive file=os.img,format=raw,if=floppy
```

## Add file to disk image
```bash
mcopy -i os.img file.txt ::FILE.TXT
```
