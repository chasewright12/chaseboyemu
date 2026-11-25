
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDLIBS =

SRC = $(wildcard src/*.c) \
      $(wildcard src/cartridge/*.c) \
      $(wildcard src/ppu/*.c)
HEADERS = $(wildcard src/*.h) \
          $(wildcard src/cartridge/*.h) \
          $(wildcard src/ppu/*.h)

OUT = gbemu

ifdef SDL
CFLAGS += -DUSE_SDL $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs)
endif

all: $(OUT)

$(OUT): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDLIBS)

clean:
	rm -f $(OUT)

.PHONY: all clean
