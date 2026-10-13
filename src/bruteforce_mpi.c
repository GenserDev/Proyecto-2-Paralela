/* Acercamiento NAIVE: el espacio [0, 2^bits) se divide en P bloques contiguos de igual tamano
 * y cada proceso recorre su bloque en orden creciente. Cuando un proceso encuentra la llave, la
 * avisa a los demas con MPI_Send; los demas la reciben con una MPI_Irecv pendiente que revisan
 * con MPI_Test cada INTERVALO_REVISION llaves.
 *
 * Fin garantizado: todos los procesos terminan en un MPI_Allreduce (aunque nadie encuentre la
 * llave), asi que no hay bloqueos si la llave no esta en el rango. */
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
    if (rango == 0) imprimirEncabezado("naive", totalProcesos, 1, &cfg, longitud);

    /* Division en bloques contiguos; el ultimo proceso absorbe el residuo */
    long limite = limiteDeBits(cfg.bits);
    long tamanoSegmento = limite / totalProcesos;
    long inicioSegmento = rango * tamanoSegmento;
    long finSegmento = (rango == totalProcesos - 1) ? limite : inicioSegmento + tamanoSegmento;

    /* Recepcion no bloqueante del aviso "llave encontrada" de cualquier proceso */
    long aviso = -1;
    MPI_Request solicitud;
    int solicitudActiva = 1;
    VERIFICAR_MPI(MPI_Irecv(&aviso, 1, MPI_LONG, MPI_ANY_SOURCE, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD,
                            &solicitud));

    VERIFICAR_MPI(MPI_Barrier(MPI_COMM_WORLD));
    double tiempoInicio = MPI_Wtime();

    long candidata = -1, probadas = 0;
    int agotado = 0;
    for (long intento = inicioSegmento; intento < finSegmento; intento++) {
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
        if (((intento - inicioSegmento) & (INTERVALO_REVISION - 1)) == INTERVALO_REVISION - 1) {
            int llego = 0;
            VERIFICAR_MPI(MPI_Test(&solicitud, &llego, MPI_STATUS_IGNORE));
            if (llego) {
                solicitudActiva = 0; /* otro proceso encontro la llave */
                break;
            }
            if (cfg.tiempoMax > 0 && MPI_Wtime() - tiempoInicio > cfg.tiempoMax) {
                agotado = 1;
                break;
            }
        }
    }
    double tiempoLocal = MPI_Wtime() - tiempoInicio;

    /* Limpieza de la recepcion pendiente (si nadie aviso, se cancela y se espera su cierre) */
    if (solicitudActiva) {
        VERIFICAR_MPI(MPI_Cancel(&solicitud));
        VERIFICAR_MPI(MPI_Wait(&solicitud, MPI_STATUS_IGNORE));
    }

    reportarResultadoMPI("naive", rango, totalProcesos, 1, &cfg, candidata, agotado, tiempoLocal, probadas,
                         probadas, cifrado, longitud);
    free(buffer);
    free(cifrado);
    MPI_Finalize();
    return 0;
}
