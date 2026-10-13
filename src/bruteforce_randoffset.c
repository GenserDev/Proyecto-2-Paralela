/* Acercamiento 3 (extra): BLOQUES CON OFFSET ALEATORIO.
 * Igual que el naive (P bloques contiguos), pero cada proceso empieza en un punto aleatorio de su
 * bloque y lo recorre de forma circular. Mantiene la simplicidad del naive y rompe la correlacion
 * fija entre la posicion de la llave y el proceso que la encuentra primero. La semilla (-s) hace
 * las corridas reproducibles. En esperanza no mejora al naive, pero evita el peor caso
 * determinista (llave justo al final del bloque 0 => speedup 1). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bf_mpi.h"
#include "des_utils.h"

#define ETIQUETA_ENCONTRADA 100

/* splitmix64: generador pseudoaleatorio pequeno y de buena calidad */
static uint64_t splitmix64(uint64_t *estado) {
    uint64_t z = (*estado += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int main(int argc, char *argv[]) {
    int rango, totalProcesos;
    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    MPI_Comm_rank(MPI_COMM_WORLD, &rango);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesos);

    Config cfg;
    unsigned char *cifrado = NULL;
    int longitud = 0;
    if (prepararEjecucion(argc, argv, rango, argv[0], " [-s semilla]", &cfg, &cifrado, &longitud) != 0) {
        MPI_Finalize();
        return 1;
    }
    unsigned char *buffer = malloc((size_t) longitud + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Rango %d: sin memoria\n", rango);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }
    if (rango == 0) imprimirEncabezado("offset_aleatorio", totalProcesos, 1, &cfg, longitud);

    long limite = limiteDeBits(cfg.bits);
    long tamanoSegmento = limite / totalProcesos;
    long inicioSegmento = rango * tamanoSegmento;
    long finSegmento = (rango == totalProcesos - 1) ? limite : inicioSegmento + tamanoSegmento;
    long largoSegmento = finSegmento - inicioSegmento;

    uint64_t estadoAleatorio = (uint64_t) cfg.semilla + (uint64_t) rango * 0x1234567ULL;
    long offset = largoSegmento > 0 ? (long) (splitmix64(&estadoAleatorio) % (uint64_t) largoSegmento) : 0;

    long aviso = -1;
    MPI_Request solicitud;
    int solicitudActiva = 1;
    VERIFICAR_MPI(MPI_Irecv(&aviso, 1, MPI_LONG, MPI_ANY_SOURCE, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD,
                            &solicitud));

    VERIFICAR_MPI(MPI_Barrier(MPI_COMM_WORLD));
    double tiempoInicio = MPI_Wtime();

    long candidata = -1, probadas = 0;
    int agotado = 0;
    for (long paso = 0; paso < largoSegmento; paso++) {
        long posicion = offset + paso;
        if (posicion >= largoSegmento) posicion -= largoSegmento; /* recorrido circular */
        long intento = inicioSegmento + posicion;
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

    reportarResultadoMPI("offset_aleatorio", rango, totalProcesos, 1, &cfg, candidata, agotado, tiempoLocal,
                         probadas, probadas, cifrado, longitud);
    free(buffer);
    free(cifrado);
    MPI_Finalize();
    return 0;
}
