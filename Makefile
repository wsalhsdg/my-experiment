CC = gcc
CFLAGS = -Wall -Wextra -I./src

SRC = $(wildcard src/*.c src/*/*.c)
OBJ = $(SRC:.c=.o)

OUT = bitcoin

all: $(OBJ)
	$(CC) $(CFLAGS) -o $(OUT) $(OBJ)

clean:
	rm -f $(OBJ) $(OUT)
