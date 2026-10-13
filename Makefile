# Proyecto 2 - Computacion Paralela y Distribuida (UVG)
# Fuerza bruta sobre DES con MPI.
#
#   make           compila todo excepto la version hibrida con OpenMP
#   make hybrid    compila bruteforce_hybrid (necesita OpenMP; en macOS: brew install libomp)
#   make todo      compila absolutamente todo
#   make clean     borra los ejecutables

UNAME := $(shell uname -s)

CC = gcc
MPICC = mpicc
CFLAGS = -Wall -Wextra -O2 -Wno-deprecated-declarations

ifeq ($(UNAME),Darwin)
  # macOS (Homebrew): OpenSSL y libomp no estan en las rutas por defecto
  OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null)
  LIBOMP_PREFIX  := $(shell brew --prefix libomp 2>/dev/null)
  SSL_CFLAGS  = -I$(OPENSSL_PREFIX)/include
  SSL_LDFLAGS = -L$(OPENSSL_PREFIX)/lib -lcrypto
  OMP_CFLAGS  ?= -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
  OMP_LDFLAGS ?= -L$(LIBOMP_PREFIX)/lib -lomp
else
  SSL_CFLAGS  =
  SSL_LDFLAGS = -lcrypto
  OMP_CFLAGS  ?= -fopenmp
  OMP_LDFLAGS ?= -fopenmp
endif

CFLAGS += $(SSL_CFLAGS)

COMUN     = src/des_utils.c src/bf_common.c
COMUN_MPI = $(COMUN) src/bf_mpi.c
ENCABEZADOS = src/des_utils.h src/bf_common.h src/bf_mpi.h

BASE = des_tool secuencial bruteforce_mpi bruteforce_interleaved bruteforce_masterworker bruteforce_randoffset

all: $(BASE)

todo: all hybrid

des_tool: src/des_tool.c $(COMUN) $(ENCABEZADOS)
	$(CC) $(CFLAGS) -o $@ src/des_tool.c $(COMUN) $(SSL_LDFLAGS)

secuencial: src/secuencial.c $(COMUN) $(ENCABEZADOS)
	$(CC) $(CFLAGS) -o $@ src/secuencial.c $(COMUN) $(SSL_LDFLAGS)

bruteforce_mpi: src/bruteforce_mpi.c $(COMUN_MPI) $(ENCABEZADOS)
	$(MPICC) $(CFLAGS) -o $@ src/bruteforce_mpi.c $(COMUN_MPI) $(SSL_LDFLAGS)

bruteforce_interleaved: src/bruteforce_interleaved.c $(COMUN_MPI) $(ENCABEZADOS)
	$(MPICC) $(CFLAGS) -o $@ src/bruteforce_interleaved.c $(COMUN_MPI) $(SSL_LDFLAGS)

bruteforce_masterworker: src/bruteforce_masterworker.c $(COMUN_MPI) $(ENCABEZADOS)
	$(MPICC) $(CFLAGS) -o $@ src/bruteforce_masterworker.c $(COMUN_MPI) $(SSL_LDFLAGS)

bruteforce_randoffset: src/bruteforce_randoffset.c $(COMUN_MPI) $(ENCABEZADOS)
	$(MPICC) $(CFLAGS) -o $@ src/bruteforce_randoffset.c $(COMUN_MPI) $(SSL_LDFLAGS)

hybrid: bruteforce_hybrid

bruteforce_hybrid: src/bruteforce_hybrid.c $(COMUN_MPI) $(ENCABEZADOS)
	$(MPICC) $(CFLAGS) $(OMP_CFLAGS) -o $@ src/bruteforce_hybrid.c $(COMUN_MPI) $(SSL_LDFLAGS) $(OMP_LDFLAGS)

clean:
	rm -f $(BASE) bruteforce_hybrid

.PHONY: all todo hybrid clean
