CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -Isrc
TARGET = editor

SRCS = src/main.c src/buffer.c src/history.c src/selection.c src/syntax.c src/config.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
