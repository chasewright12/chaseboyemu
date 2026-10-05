#include "cart.h"
#include <stdio.h>
#include <stdlib.h>

int cart_load(Cart *cart, const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		perror("Erro ao abrir a ROM");
		return 0;
	}

	fseek(f, 0, SEEK_END);
	cart->size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (cart->size < 0x150)
	{
		fprintf(stderr, "ROM invalida: arquivo menor que o cabecalho (0x150 bytes)\n");
		fclose(f);
		return 0;
	}

	cart->data = malloc(cart->size);
	if (!cart->data)
	{
		fclose(f);
		return 0;
	}

	if (fread(cart->data, 1, cart->size, f) != cart->size)
	{
		fprintf(stderr, "Erro ao ler a ROM\n");
		free(cart->data);
		fclose(f);
		return 0;
	}

	fclose(f);
	return 1;
}

void cart_free(Cart *cart)
{
	free(cart->data);
	cart->data = NULL;
	cart->size = 0;
}

void cart_print_header(const Cart *cart)
{
	char title[16] = {0};
	for (int i = 0; i < 15; i++)
	{
		title[i] = (char)cart->data[0x0134 + i];
	}

	uint8_t cgb = cart->data[0x0143];
	const char *modo = "Game Boy (DMG)";
	if (cgb == 0xC0)
		modo = "Game Boy Color (somente CGB)";
	else if (cgb & 0x80)
		modo = "Game Boy Color (compativel com DMG)";

	printf("Titulo: %s\n", title);
	printf("Modo: %s\n", modo);
	printf("Tipo do cartucho: 0x%02X\n", cart->data[0x0147]);
	printf("Codigo do tamanho da ROM: 0x%02X\n", cart->data[0x0148]);
	printf("Tamanho do arquivo: %zu bytes\n", cart->size);
}