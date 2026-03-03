#define VGA_MEMORY (char*)0xB8000

#define WHITE_ON_BLACK 0x0F

void kernel_main(){
	char *vga = VGA_MEMORY;
	const char *msg = "Hello From Kernel!";

	for(int i = 0; i < 80 * 25 * 2; i += 2){
		vga[i]	= ' '; //character
		vga[i + 1] = WHITE_ON_BLACK; //color ili colour hz
	}

	for (int i = 0; msg[i] != '\0'; i++){
		vga[i * 2]	= msg[i];
		vga[i * 2 + 1]  = WHITE_ON_BLACK;
	}

	while (1) {}

}
