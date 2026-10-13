/* Extra: HIBRIDO MPI + OpenMP sobre el esquema ENTRELAZADO.
 * Hay P procesos MPI y T hilos OpenMP por proceso: W = P*T trabajadores en total. El trabajador
 * global w = rango*T + hilo prueba las llaves w, w+W, w+2W, ... (mismo entrelazado, con salto W).
 *
 *  - Memoria compartida dentro de un proceso: los hilos se avisan con una bandera atomica.
 *  - Memoria distribuida entre procesos: MPI_Send / MPI_Irecv como en las demas versiones.
 *  - MPI se inicia con MPI_THREAD_FUNNELED: solo el hilo 0 (hilo principal) llama a MPI dentro de
 *    la region paralela (MPI_Test); el envio del aviso lo hace el hilo principal al salir de ella.
 *  - Cada hilo tiene su propio buffer de descifrado (no hay estado compartido en el ciclo caliente). */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "bf_mpi.h"
#include "des_utils.h"

#define ETIQUETA_ENCONTRADA 100

int main(int argc, char *argv[]) {
    int rango, totalProcesos, nivelSoportado = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &nivelSoportado);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    MPI_Comm_rank(MPI_COMM_WORLD, &rango);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesos);
    if (nivelSoportado < MPI_THREAD_FUNNELED) {
        if (rango == 0) fprintf(stderr, "Error: esta instalacion de MPI no soporta MPI_THREAD_FUNNELED\n");
        MPI_Finalize();
        return 1;
    }

    Config cfg;
    unsigned char *cifrado = NULL;
    int longitud = 0;
    if (prepararEjecucion(argc, argv, rango, argv[0], " [-h hilos]", &cfg, &cifrado, &longitud) != 0) {
        MPI_Finalize();
        return 1;
    }

    /* Todos los procesos deben usar el mismo numero de hilos (el del rango 0) para que el
     * entrelazado global w = rango*T + hilo no deje huecos ni se solape. */
    int hilos = cfg.hilos > 0 ? cfg.hilos : omp_get_max_threads();
    VERIFICAR_MPI(MPI_Bcast(&hilos, 1, MPI_INT, 0, MPI_COMM_WORLD));

    if (rango == 0) imprimirEncabezado("hibrido_entrelazado", totalProcesos, hilos, &cfg, longitud);

    long limite = limiteDeBits(cfg.bits);
    long trabajadoresTotales = (long) totalProcesos * hilos;

    long aviso = -1;
    MPI_Request solicitud;
    int solicitudActiva = 1;
    VERIFICAR_MPI(MPI_Irecv(&aviso, 1, MPI_LONG, MPI_ANY_SOURCE, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD,
                            &solicitud));

    int detener = 0, agotado = 0, hilosTerminados = 0, falloMemoria = 0;
    long hallazgoLocal = -1, iterMaxLocal = 0, iterSumaLocal = 0;

    VERIFICAR_MPI(MPI_Barrier(MPI_COMM_WORLD));
    double tiempoInicio = MPI_Wtime();

#pragma omp parallel num_threads(hilos)
    {
        int hilo = omp_get_thread_num();
        unsigned char *buffer = malloc((size_t) longitud + 1);
        long probadas = 0;

        if (buffer == NULL) {
#pragma omp atomic write
            falloMemoria = 1;
#pragma omp atomic write
            detener = 1;
        } else {
            for (long intento = (long) rango * hilos + hilo; intento < limite; intento += trabajadoresTotales) {
                probadas++;
                if (probarLlaveBuffer(intento, cifrado, longitud, cfg.palabraClave, buffer)) {
#pragma omp critical(hallazgo)
                    {
                        if (hallazgoLocal < 0 || intento < hallazgoLocal) hallazgoLocal = intento;
                    }
#pragma omp atomic write
                    detener = 1;
                    break;
                }
                if ((probadas & (INTERVALO_REVISION - 1)) == 0) {
                    int parar;
#pragma omp atomic read
                    parar = detener;
                    if (parar) break;
                    if (hilo == 0) { /* solo el hilo principal habla con MPI */
                        int llego = 0;
                        VERIFICAR_MPI(MPI_Test(&solicitud, &llego, MPI_STATUS_IGNORE));
                        if (llego) {
                            solicitudActiva = 0;
#pragma omp atomic write
                            detener = 1;
                        } else if (cfg.tiempoMax > 0 && MPI_Wtime() - tiempoInicio > cfg.tiempoMax) {
#pragma omp atomic write
                            agotado = 1;
#pragma omp atomic write
                            detener = 1;
                        }
                    }
                }
            }
            free(buffer);
        }

#pragma omp critical(contadores)
        {
            if (probadas > iterMaxLocal) iterMaxLocal = probadas;
            iterSumaLocal += probadas;
        }

        if (hilo != 0) {
#pragma omp atomic update
            hilosTerminados++;
        } else {
            /* El hilo 0 sigue atendiendo MPI mientras los demas hilos terminan su parte, para no
             * perder un aviso de otro proceso si el hilo 0 acabo antes que el resto. */
            for (;;) {
                int parar, terminados;
#pragma omp atomic read
                parar = detener;
#pragma omp atomic read
                terminados = hilosTerminados;
                if (parar || terminados >= omp_get_num_threads() - 1) break;
                int llego = 0;
                VERIFICAR_MPI(MPI_Test(&solicitud, &llego, MPI_STATUS_IGNORE));
                if (llego) {
                    solicitudActiva = 0;
#pragma omp atomic write
                    detener = 1;
                }
            }
        }
    }
    double tiempoLocal = MPI_Wtime() - tiempoInicio;

    if (falloMemoria) {
        fprintf(stderr, "Rango %d: sin memoria en un hilo\n", rango);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    /* Fuera de la region paralela: el hilo principal avisa a los demas procesos */
    if (hallazgoLocal >= 0) {
        for (int destino = 0; destino < totalProcesos; destino++) {
            if (destino != rango) {
                VERIFICAR_MPI(MPI_Send(&hallazgoLocal, 1, MPI_LONG, destino, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD));
            }
        }
    }
    if (solicitudActiva) {
        VERIFICAR_MPI(MPI_Cancel(&solicitud));
        VERIFICAR_MPI(MPI_Wait(&solicitud, MPI_STATUS_IGNORE));
    }

    reportarResultadoMPI("hibrido_entrelazado", rango, totalProcesos, hilos, &cfg, hallazgoLocal, agotado,
                         tiempoLocal, iterMaxLocal, iterSumaLocal, cifrado, longitud);
    free(cifrado);
    MPI_Finalize();
    return 0;
}
