CC = gcc
CFLAGS = -g -Wall -Wno-return-type # -Werror
LDFLAGS = -pthread

BINARIES = servidor_mono servidor_multi cliente

all: $(BINARIES)

servidor_mono: servidor_mono.c biblioteca.o biblioteca.h
	$(CC) $(LDFLAGS) $(CFLAGS) servidor_mono.c biblioteca.o -o servidor_mono -lpthread

servidor_multi: servidor_multi.c biblioteca.o biblioteca.h
	$(CC) $(LDFLAGS) $(CFLAGS) servidor_multi.c biblioteca.o -o servidor_multi

cliente: cliente.c biblioteca.o biblioteca.h
	$(CC) $(LDFLAGS) $(CFLAGS) cliente.c biblioteca.o -o cliente

biblioteca.o: biblioteca.c biblioteca.h
	$(CC) $(CFLAGS) -c biblioteca.c

clean:
	rm -f *.o
	rm -f $(BINARIES)
