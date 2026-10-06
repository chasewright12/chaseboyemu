CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDLIBS =
SRC = $(wildcard src/*.c)
OUT = gbemu

# Make SDL = 1 compiles with window (NEED libsd12-dev)
ifdef SDL
CFLAGS += -DUSE_SDL $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs)
endif

all: $(OUT)

$(OUT): $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDLIBS)

clean:
	rm -f $(OUT)