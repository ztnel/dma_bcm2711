CC=gcc
CFLAGS=-g -W -Wall -std=c99

dma: dma.o
	$(CC) -o dma dma.o

clean:
	rm -f ./*.o ./dma