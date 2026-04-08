CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L

monte_carlo: main.o worker.o common.o
	$(CC) $(CFLAGS) -o monte_carlo main.o worker.o common.o

main.o: main.c worker.h common.h
	$(CC) $(CFLAGS) -c main.c

worker.o: worker.c worker.h common.h
	$(CC) $(CFLAGS) -c worker.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

clean:
	rm -f *.o monte_carlo

.PHONY: clean
