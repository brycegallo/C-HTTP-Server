CC=gcc
main: main.c
	$(CC) main.c -o main -Wall -Wextra -pedantic -std=gnu99
