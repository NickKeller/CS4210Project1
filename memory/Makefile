CFLAGS=-std=c99 -Wall -g -Werror

all:
	gcc -o memory_coordinator memory_coordinator.c `pkg-config libvirt --libs` $(CFLAGS)
clean:
	rm -f memory_coordinator
