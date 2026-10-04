CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
SRC = $(wildcard src/*.c)
OUT = gbemu

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)
