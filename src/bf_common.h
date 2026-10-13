#ifndef BF_COMMON_H
#define BF_COMMON_H

#include <stddef.h>

/* Cada cuantas llaves un trabajador revisa si debe detenerse (señal MPI, tiempo limite).
 * Debe ser potencia de 2. */
#define INTERVALO_REVISION 4096L

#define MAX_BITS_LLAVE 56            /* DES: 56 bits utiles de llave */
#define TAMANO_MAX_ARCHIVO (1L << 20) /* 1 MiB: limite defensivo de lectura */
#define MIN_LONGITUD_PALABRA 8        /* menos que esto arriesga falsos positivos */

typedef struct {
    const char *archivoCifrado;  /* ruta del archivo cifrado (binario) */
    const char *palabraClave;    /* subcadena conocida del texto plano */
    int bits;                    /* el espacio de busqueda es [0, 2^bits) */
    double tiempoMax;            /* segundos; 0 = sin limite */
    long chunk;                  /* tamano de chunk (maestro-trabajador); 0 = automatico */
    unsigned long semilla;       /* semilla del offset aleatorio */
    int hilos;                   /* hilos OpenMP por proceso; 0 = OMP_NUM_THREADS */
} Config;

/* Lee argumentos: <archivo_cifrado> <palabra_clave> <bits> [-t seg] [-c chunk] [-s semilla] [-h hilos].
 * Devuelve 0 si todo es valido; -1 en error (mensaje en msgError). */
int parsearArgumentos(int argc, char **argv, Config *cfg, char *msgError, size_t tam);

void imprimirUso(const char *programa, const char *extras);

/* Lee un archivo completo a memoria (malloc; el llamador hace free). Devuelve NULL en error.
 * Si exigirMultiplo8 es distinto de 0, el tamano debe ser multiplo de 8 (bloques DES). */
unsigned char *leerArchivoBinario(const char *ruta, int *longitud, int exigirMultiplo8,
                                  char *msgError, size_t tam);

/* Numero de llaves del espacio de busqueda: 2^bits. */
long limiteDeBits(int bits);

void imprimirEncabezado(const char *algoritmo, int procesos, int hilos, const Config *cfg, int longitud);

/* llave < 0 significa "no encontrada". agotado != 0 indica que se corto por tiempo maximo. */
void imprimirResultado(const char *algoritmo, int procesos, int hilos, const Config *cfg,
                       long llave, int agotado, double segundos, long iterMax, long iterTotal,
                       const unsigned char *cifrado, int longitud);

#endif
