
CC = gcc
CFLAGS = -Wall -Wextra
TARGET = bin/ls
SRC = src/lsv1.2.0.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
