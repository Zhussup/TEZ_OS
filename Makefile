all: os.img

kernel/entry.o: kernel/entry.asm
	nasm -f elf32 kernel/entry.asm -o kernel/entry.o

kernel/keyboard_isr.o: kernel/keyboard_isr.asm
	nasm -f elf32 kernel/keyboard_isr.asm -o kernel/keyboard_isr.o

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

kernel/kernel.bin: kernel/entry.o kernel/keyboard_isr.o kernel/kernel.o kernel/idt.o kernel/keyboard.o kernel/ata.o kernel/fat12.o kernel/tuze.o
	ld -m elf_i386 -T kernel/linker.ld -o kernel/kernel.elf \
		kernel/entry.o kernel/keyboard_isr.o kernel/kernel.o \
		kernel/idt.o kernel/keyboard.o kernel/ata.o kernel/fat12.o kernel/tuze.o
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
