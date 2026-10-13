#include "bf_mpi.h"

#include <stdio.h>
#include <stdlib.h>

int prepararEjecucion(int argc, char **argv, int rango, const char *programa, const char *usoExtra,
                      Config *cfg, unsigned char **cifrado, int *longitud) {
    char error[300] = "";
    /* Todos los rangos reciben los mismos argumentos, asi que todos fallan (o no) de forma identica */
    if (parsearArgumentos(argc, argv, cfg, error, sizeof error) != 0) {
        if (rango == 0) {
            fprintf(stderr, "Error: %s\n\n", error);
            imprimirUso(programa, usoExtra);
        }
        return -1;
    }

    int largo = -1;
    unsigned char *datos = NULL;
    if (rango == 0) {
        datos = leerArchivoBinario(cfg->archivoCifrado, &largo, 1, error, sizeof error);
        if (datos == NULL) {
            fprintf(stderr, "Error: %s\n", error);
            largo = -1;
        }
    }
    VERIFICAR_MPI(MPI_Bcast(&largo, 1, MPI_INT, 0, MPI_COMM_WORLD));
    if (largo <= 0) {
        free(datos);
        return -1;
    }
    if (rango != 0) {
        datos = malloc((size_t) largo);
        if (datos == NULL) {
            fprintf(stderr, "Rango %d: sin memoria para el texto cifrado\n", rango);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }
    VERIFICAR_MPI(MPI_Bcast(datos, largo, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD));
    *cifrado = datos;
    *longitud = largo;
    return 0;
}

void reportarResultadoMPI(const char *algoritmo, int rango, int procesos, int hilos, const Config *cfg,
                          long candidata, int agotadoLocal, double segundosLocal, long iterMaxLocal,
                          long iterSumaLocal, const unsigned char *cifrado, int longitud) {
    long llave = -1, iterMax = 0, iterTotal = 0;
    int agotado = 0;
    double segundos = 0;
    VERIFICAR_MPI(MPI_Allreduce(&candidata, &llave, 1, MPI_LONG, MPI_MAX, MPI_COMM_WORLD));
    VERIFICAR_MPI(MPI_Reduce(&agotadoLocal, &agotado, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD));
    VERIFICAR_MPI(MPI_Reduce(&segundosLocal, &segundos, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD));
    VERIFICAR_MPI(MPI_Reduce(&iterMaxLocal, &iterMax, 1, MPI_LONG, MPI_MAX, 0, MPI_COMM_WORLD));
    VERIFICAR_MPI(MPI_Reduce(&iterSumaLocal, &iterTotal, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD));
    if (rango == 0) {
        imprimirResultado(algoritmo, procesos, hilos, cfg, llave, agotado, segundos, iterMax, iterTotal,
                          cifrado, longitud);
    }
}
