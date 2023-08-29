CC = gcc
LD = gcc
CFLAGS = -O2 -std=gnu17 -Wall
LDFLAGS = -g
all: pw_stereomix

clean:
	rm -rf pw_stereomix main.o node_queue.o

pw_stereomix: main.o node_queue.o
	$(LD) $(LDFLAGS) `pkg-config --libs libpipewire-0.3` -fpie $^ -o $@

main.o:	main.c
	$(CC) $(CFLAGS) `pkg-config --cflags libpipewire-0.3` -fPIC -c $^ -o $@

node_queue.o: node_queue.c
	$(CC) $(CFLAGS) `pkg-config --cflags libpipewire-0.3` -fPIC -c $^ -o $@
