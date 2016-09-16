CFLAGS=-std=c99 -Wall -g -Werror

all:
	gcc -o vcpu_scheduler vcpu_scheduler.c `pkg-config libvirt --libs` $(CFLAGS)
clean:
	rm -f vcpu_scheduler
