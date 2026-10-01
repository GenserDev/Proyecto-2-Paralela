OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null)

CC = gcc
MPICC = mpicc
CFLAGS = -Wall -O2 -Wno-deprecated-declarations -I$(OPENSSL_PREFIX)/include
LDFLAGS = -L$(OPENSSL_PREFIX)/lib -lcrypto

all: secuencial bruteforce_mpi

secuencial: src/secuencial.c src/des_utils.c
	$(CC) $(CFLAGS) -o secuencial src/secuencial.c src/des_utils.c $(LDFLAGS)

bruteforce_mpi: src/bruteforce_mpi.c src/des_utils.c
	$(MPICC) $(CFLAGS) -o bruteforce_mpi src/bruteforce_mpi.c src/des_utils.c $(LDFLAGS)

clean:
	rm -f secuencial bruteforce_mpi

.PHONY: all clean
