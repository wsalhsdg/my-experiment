# 编译器和选项
CC = gcc
CFLAGS = -Wall -Wextra -O2 -DUSE_SECP256K1 -I./src -I./secp256k1/include -g

# 静态库和外部库
LIBS = ./secp256k1/lib/libsecp256k1.a -lssl -lcrypto -lpthread

# 源文件
SRC = $(wildcard src/*.c src/*/*.c)
OBJ = $(SRC:.c=.o)

# 输出文件
OUT = bitcoin

# 默认目标
all: $(OUT)

# 链接生成可执行文件
$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# 清理
clean:
	rm -f $(OBJ) $(OUT)
