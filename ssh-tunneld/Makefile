CFLAGS=-Wall -Wextra -std=c11 -pedantic
OBJECTS:=$(patsubst %.c,%.o,$(wildcard *.c))

all: ssh-tunneld

ssh-tunneld: $(OBJECTS)
	$(CC) -o $@ $^

clean:
	rm -f ssh-tunneld
	rm -f $(OBJECTS)
