all: kernel/kernel.bin

kernel/entry.o: kernel/entry.asm
	nasm -f elf32 kernel/entry.asm -o kernel/entry.o

kernel/kernel.o: kernel/kernel.c
	gcc -m32 -ffreestanding -fno-builtin -nostdlib -c kernel/kernel.c -o kernel/kernel.o

kernel/kernel.bin: kernel/entry.o kernel/kernel.o
	ld -m elf_i386 -T kernel/linker.ld -o kernel/kernel.bin kernel/entry.o kernel/kernel.o

clean:
	rm -f kernel/*.o kernel/kernel.bin os.img
