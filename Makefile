.PHONY: all clean ssh-tunneld ssh-tunnelc

all: ssh-tunneld ssh-tunnelc

ssh-tunneld:
	$(MAKE) -C ssh-tunneld/

ssh-tunnelc:
	$(MAKE) -C ssh-tunnelc/

clean:
	$(MAKE) -C ssh-tunneld/ clean
	$(MAKE) -C ssh-tunnelc/ clean

