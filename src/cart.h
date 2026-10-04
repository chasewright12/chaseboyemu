#ifndef CART_H
#define CART_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
} Cart;

int cart_load(Cart *cart, const char *path);
void cart_free(Cart *cart);
void cart_print_header(const Cart *cart);

#endif