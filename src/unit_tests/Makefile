CC=gcc
CFLAGS=-I. -Wall
BIN_NAME=tests

build: ringbuffer_test.o
		$(CC) -o $(BIN_NAME) ringbuffer_test.c $(CFLAGS)

test: build
		./$(BIN_NAME)

clean:
	rm *.o
	rm $(BIN_NAME)