#include "cart.h"
#include "mem.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Uso: %s <rom.gb>\n", argv[0]);
		return 1;
	}

	Cart cart;
	if (!cart_load(&cart, argv[1]))
	{
		return 1;
	}
	cart_print_header(&cart);

	Mem mem;
	mem_init(&mem, &cart);

	printf("ROM[0x0100] = 0x%02X\n", mem_read(&mem, 0x0100));

	mem_write(&mem, 0xC000, 0x42);
	printf("WRAM[0xC000] = 0x%02X (esperado 0x42)\n", mem_read(&mem, 0xC000));
	printf("Echo[0xE000] = 0x%02X (esperado 0x42)\n", mem_read(&mem, 0xE000));

	mem_write(&mem, 0x0100, 0x99);
	printf("ROM[0x0100] apos escrita = 0x%02X (nao deve mudar)\n", mem_read(&mem, 0x0100));

	cart_free(&cart);
	return 0;
}