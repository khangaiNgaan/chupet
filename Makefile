CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99 -Iinclude -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_DARWIN_C_SOURCE
LDFLAGS = 

TARGET = chupet
SRCS = src/main.c src/config.c src/http_client.c src/linenoise.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
