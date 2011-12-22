CFLAGS=-Wall -m32 -g
all:
	cc main.c -o main
clean:
	rm -rf testout.txt display.txt main
