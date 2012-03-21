CFLAGS=-Wall -Wextra -std=c99 -pedantic
CC=gcc
OBJECTS:=$(patsubst %.c,%.o,$(wildcard *.c))

all: ssh-tunneld

ssh-tunneld: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f ssh-tunneld
	rm -f $(OBJECTS)
