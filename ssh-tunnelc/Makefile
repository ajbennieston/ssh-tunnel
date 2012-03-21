CFLAGS=-Wall -Wextra -std=c99 -pedantic
CC=gcc
OBJECTS:=$(patsubst %.c,%.o,$(wildcard *.c))

all: ssh-tunnelc

ssh-tunnelc: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f ssh-tunnelc
	rm -f $(OBJECTS)
