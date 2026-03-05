all: os.img

kernel/entry.o: kernel/entry.asm
	nasm -f elf32 kernel/entry.asm -o kernel/entry.o

kernel/keyboard_isr.o: kernel/keyboard_isr.asm
	nasm -f elf32 kernel/keyboard_isr.asm -o kernel/keyboard_isr.o

kernel/kernel.o: kernel/kernel.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -c kernel/kernel.c -o kernel/kernel.o

kernel/idt.o: kernel/idt.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -c kernel/idt.c -o kernel/idt.o

kernel/keyboard.o: kernel/keyboard.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -c kernel/keyboard.c -o kernel/keyboard.o

kernel/kernel.bin: kernel/entry.o kernel/keyboard_isr.o kernel/kernel.o kernel/idt.o kernel/keyboard.o
	ld -m elf_i386 -T kernel/linker.ld -o kernel/kernel.elf \
		kernel/entry.o kernel/keyboard_isr.o kernel/kernel.o kernel/idt.o kernel/keyboard.o
	objcopy -O binary kernel/kernel.elf kernel/kernel.bin

boot/boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o boot/boot.bin

os.img: boot/boot.bin kernel/kernel.bin
	dd if=/dev/zero of=os.img bs=512 count=2880
	dd if=boot/boot.bin of=os.img conv=notrunc
	dd if=kernel/kernel.bin of=os.img bs=512 seek=1 conv=notrunc

clean:
	rm -f kernel/*.o kernel/*.elf kernel/kernel.bin os.img
