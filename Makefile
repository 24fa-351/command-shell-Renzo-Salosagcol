xsh: main.o main.c
	gcc -Wall -Wextra -std=c99 -o xsh main.o main.c

clean:
	rm -f xsh main.o

.PHONY: all clean
