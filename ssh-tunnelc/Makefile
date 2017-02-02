CFLAGS=-Wall -Wextra -std=c11 -pedantic
OBJECTS:=$(patsubst %.c,%.o,$(wildcard *.c))

all: ssh-tunnelc

ssh-tunnelc: $(OBJECTS)
	$(CC) -o $@ $^

clean:
	rm -f ssh-tunnelc
	rm -f $(OBJECTS)
