/* Acercamiento 1: ENTRELAZADO (round-robin / striding).
 * El proceso r prueba las llaves r, r+P, r+2P, ... En vez de bloques contiguos, cada proceso
 * recorre TODO el espacio con salto P. Asi la llave real queda "repartida" entre procesos sin
 * importar su valor numerico, y el tiempo paralelo deja de depender de en que bloque cae la
 * llave: todos los procesos avanzan sobre el mismo frente de busqueda (0, 1, 2, ...).
 * Tpar ~ (llave / P) iteraciones para cualquier llave => speedup ~ P de forma consistente. */
#include <stdio.h>
#include <stdlib.h>

#include "bf_mpi.h"
#include "des_utils.h"

#define ETIQUETA_ENCONTRADA 100

int main(int argc, char *argv[]) {
    int rango, totalProcesos;
    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    MPI_Comm_rank(MPI_COMM_WORLD, &rango);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesos);

    Config cfg;
    unsigned char *cifrado = NULL;
    int longitud = 0;
    if (prepararEjecucion(argc, argv, rango, argv[0], "", &cfg, &cifrado, &longitud) != 0) {
        MPI_Finalize();
        return 1;
    }
    unsigned char *buffer = malloc((size_t) longitud + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Rango %d: sin memoria\n", rango);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }
    if (rango == 0) imprimirEncabezado("entrelazado", totalProcesos, 1, &cfg, longitud);

    long limite = limiteDeBits(cfg.bits);

    long aviso = -1;
    MPI_Request solicitud;
    int solicitudActiva = 1;
    VERIFICAR_MPI(MPI_Irecv(&aviso, 1, MPI_LONG, MPI_ANY_SOURCE, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD,
                            &solicitud));

    VERIFICAR_MPI(MPI_Barrier(MPI_COMM_WORLD));
    double tiempoInicio = MPI_Wtime();

    long candidata = -1, probadas = 0;
    int agotado = 0;
    for (long intento = rango; intento < limite; intento += totalProcesos) {
        probadas++;
        if (probarLlaveBuffer(intento, cifrado, longitud, cfg.palabraClave, buffer)) {
            candidata = intento;
            for (int destino = 0; destino < totalProcesos; destino++) {
                if (destino != rango) {
                    VERIFICAR_MPI(MPI_Send(&intento, 1, MPI_LONG, destino, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD));
                }
            }
            break;
        }
        if ((probadas & (INTERVALO_REVISION - 1)) == 0) {
            int llego = 0;
            VERIFICAR_MPI(MPI_Test(&solicitud, &llego, MPI_STATUS_IGNORE));
            if (llego) {
                solicitudActiva = 0;
                break;
            }
            if (cfg.tiempoMax > 0 && MPI_Wtime() - tiempoInicio > cfg.tiempoMax) {
                agotado = 1;
                break;
            }
        }
    }
    double tiempoLocal = MPI_Wtime() - tiempoInicio;

    if (solicitudActiva) {
        VERIFICAR_MPI(MPI_Cancel(&solicitud));
        VERIFICAR_MPI(MPI_Wait(&solicitud, MPI_STATUS_IGNORE));
    }

    reportarResultadoMPI("entrelazado", rango, totalProcesos, 1, &cfg, candidata, agotado, tiempoLocal,
                         probadas, probadas, cifrado, longitud);
    free(buffer);
    free(cifrado);
    MPI_Finalize();
    return 0;
}
