all: os.img

kernel/entry.o: kernel/entry.asm
	nasm -f elf32 kernel/entry.asm -o kernel/entry.o
kernel/keyboard_isr.o: kernel/keyboard_isr.asm
	nasm -f elf32 kernel/keyboard_isr.asm -o kernel/keyboard_isr.o
kernel/pit_isr.o: kernel/pit_isr.asm
	nasm -f elf32 kernel/pit_isr.asm -o kernel/pit_isr.o

kernel/kernel.o: kernel/kernel.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/kernel.c -o kernel/kernel.o
kernel/idt.o: kernel/idt.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/idt.c -o kernel/idt.o
kernel/keyboard.o: kernel/keyboard.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/keyboard.c -o kernel/keyboard.o
kernel/ata.o: kernel/ata.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/ata.c -o kernel/ata.o
kernel/fat12.o: kernel/fat12.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/fat12.c -o kernel/fat12.o
kernel/tuze.o: kernel/tuze.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/tuze.c -o kernel/tuze.o
kernel/calc.o: kernel/calc.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/calc.c -o kernel/calc.o
kernel/rtc.o: kernel/rtc.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/rtc.c -o kernel/rtc.o
kernel/pit.o: kernel/pit.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/pit.c -o kernel/pit.o
kernel/tez_lexer.o: kernel/tez_lexer.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/tez_lexer.c -o kernel/tez_lexer.o
kernel/tez_parser.o: kernel/tez_parser.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/tez_parser.c -o kernel/tez_parser.o
kernel/tez_codegen.o: kernel/tez_codegen.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/tez_codegen.c -o kernel/tez_codegen.o
kernel/tez.o: kernel/tez.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -fno-pic -c kernel/tez.c -o kernel/tez.o

kernel/kernel.bin: kernel/entry.o kernel/keyboard_isr.o kernel/pit_isr.o \
                   kernel/kernel.o kernel/idt.o kernel/keyboard.o \
                   kernel/ata.o kernel/fat12.o kernel/tuze.o kernel/calc.o \
                   kernel/rtc.o kernel/pit.o \
                   kernel/tez_lexer.o kernel/tez_parser.o kernel/tez_codegen.o kernel/tez.o
	ld -m elf_i386 -T kernel/linker.ld -o kernel/kernel.elf \
		kernel/entry.o kernel/keyboard_isr.o kernel/pit_isr.o \
		kernel/kernel.o kernel/idt.o kernel/keyboard.o \
		kernel/ata.o kernel/fat12.o kernel/tuze.o kernel/calc.o \
		kernel/rtc.o kernel/pit.o \
		kernel/tez_lexer.o kernel/tez_parser.o kernel/tez_codegen.o kernel/tez.o
	objcopy -O binary kernel/kernel.elf kernel/kernel.bin

boot/boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o boot/boot.bin

os.img: boot/boot.bin kernel/kernel.bin
	dd if=/dev/zero of=os.img bs=512 count=2880
	mkdosfs -F 12 -n "TEZ_OS" os.img
	mcopy -i os.img hello.txt ::HELLO.TXT
	dd if=boot/boot.bin of=os.img conv=notrunc bs=512 count=1
	dd if=kernel/kernel.bin of=os.img bs=512 seek=50 conv=notrunc

clean:
	rm -f kernel/*.o kernel/*.elf kernel/kernel.bin os.img