/* Acercamiento 2: MAESTRO-TRABAJADOR con chunks dinamicos.
 * El espacio [0, 2^bits) se corta en muchos chunks pequenos (consecutivos). El rango 0 es el
 * maestro: reparte chunks bajo demanda en orden creciente, de modo que el frente de busqueda
 * avanza compacto (0, 1, 2, ...) y ningun proceso queda con un bloque enorme "desafortunado".
 * El maestro tambien trabaja: entre atencion y atencion de solicitudes prueba rebanadas de un
 * chunk propio, asi se aprovechan los P procesos.
 *
 * Protocolo (todo con MPI_Send / MPI_Recv, sin interbloqueos):
 *   trabajador -> maestro  (ETIQUETA_SOLICITUD):  llave encontrada en el chunk anterior, o -1
 *   maestro -> trabajador  (ETIQUETA_ASIGNACION): indice de chunk, o -1 = detenerse
 * Cada solicitud sirve a la vez como "resultado del chunk anterior" y "pido otro chunk".
 * Tiempo reportado: hasta que el maestro conoce la llave (los trabajadores terminan su chunk
 * en curso despues, sin afectar el tiempo de descubrimiento). */
#include <stdio.h>
#include <stdlib.h>

#include "bf_mpi.h"
#include "des_utils.h"

#define ETIQUETA_SOLICITUD 1
#define ETIQUETA_ASIGNACION 2
#define REBANADA_MAESTRO 2048L /* llaves que prueba el maestro antes de atender solicitudes */

static long minimo(long a, long b) { return a < b ? a : b; }

int main(int argc, char *argv[]) {
    int rango, totalProcesos;
    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    MPI_Comm_rank(MPI_COMM_WORLD, &rango);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesos);

    Config cfg;
    unsigned char *cifrado = NULL;
    int longitud = 0;
    if (prepararEjecucion(argc, argv, rango, argv[0], " [-c tamano_chunk]", &cfg, &cifrado, &longitud) != 0) {
        MPI_Finalize();
        return 1;
    }
    unsigned char *buffer = malloc((size_t) longitud + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Rango %d: sin memoria\n", rango);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    long limite = limiteDeBits(cfg.bits);
    long chunk = cfg.chunk;
    if (chunk == 0) { /* automatico: ~256 chunks por proceso, acotado a [1024, 2^20] */
        chunk = limite / ((long) totalProcesos * 256);
        if (chunk < 1024) chunk = 1024;
        if (chunk > (1L << 20)) chunk = 1L << 20;
    }
    if (chunk > limite) chunk = limite;
    long totalChunks = (limite + chunk - 1) / chunk;

    if (rango == 0) {
        imprimirEncabezado("maestro_trabajador", totalProcesos, 1, &cfg, longitud);
        printf("Tamano de chunk:   %ld llaves (%ld chunks)\n", chunk, totalChunks);
        fflush(stdout);
    }

    VERIFICAR_MPI(MPI_Barrier(MPI_COMM_WORLD));
    double tiempoInicio = MPI_Wtime();

    long candidata = -1, probadas = 0;
    int agotado = 0;
    double tiempoLocal = 0;

    if (rango == 0) {
        long siguienteChunk = 0;
        int detener = 0;
        int trabajadoresActivos = totalProcesos - 1;
        int maestroActivo = 1, tieneChunk = 0;
        long posicion = 0, finChunk = 0;
        double tiempoHallazgo = 0;

        while (trabajadoresActivos > 0 || maestroActivo) {
            /* 1) Atender solicitudes: sin bloquear mientras el maestro tenga trabajo propio; si ya
             *    no tiene, se bloquea esperando a los trabajadores restantes. */
            while (trabajadoresActivos > 0) {
                int hay = 0;
                MPI_Status estado;
                if (maestroActivo) {
                    VERIFICAR_MPI(MPI_Iprobe(MPI_ANY_SOURCE, ETIQUETA_SOLICITUD, MPI_COMM_WORLD, &hay, &estado));
                } else {
                    VERIFICAR_MPI(MPI_Probe(MPI_ANY_SOURCE, ETIQUETA_SOLICITUD, MPI_COMM_WORLD, &estado));
                    hay = 1;
                }
                if (!hay) break;

                long resultado;
                VERIFICAR_MPI(MPI_Recv(&resultado, 1, MPI_LONG, estado.MPI_SOURCE, ETIQUETA_SOLICITUD,
                                       MPI_COMM_WORLD, MPI_STATUS_IGNORE));
                if (resultado >= 0 && candidata < 0) {
                    candidata = resultado;
                    tiempoHallazgo = MPI_Wtime() - tiempoInicio;
                    detener = 1;
                }
                if (!detener && cfg.tiempoMax > 0 && MPI_Wtime() - tiempoInicio > cfg.tiempoMax) {
                    agotado = 1;
                    detener = 1;
                }
                long asignacion = -1;
                if (!detener && siguienteChunk < totalChunks) {
                    asignacion = siguienteChunk++;
                } else {
                    trabajadoresActivos--; /* este trabajador recibe la orden de detenerse */
                }
                VERIFICAR_MPI(MPI_Send(&asignacion, 1, MPI_LONG, estado.MPI_SOURCE, ETIQUETA_ASIGNACION,
                                       MPI_COMM_WORLD));
            }

            /* 2) Trabajo propio del maestro: una rebanada de su chunk actual */
            if (maestroActivo) {
                if (detener) {
                    maestroActivo = 0;
                } else {
                    if (!tieneChunk) {
                        if (siguienteChunk < totalChunks) {
                            long indice = siguienteChunk++;
                            posicion = indice * chunk;
                            finChunk = minimo(posicion + chunk, limite);
                            tieneChunk = 1;
                        } else {
                            maestroActivo = 0;
                        }
                    }
                    if (tieneChunk) {
                        long finRebanada = minimo(posicion + REBANADA_MAESTRO, finChunk);
                        for (; posicion < finRebanada; posicion++) {
                            probadas++;
                            if (probarLlaveBuffer(posicion, cifrado, longitud, cfg.palabraClave, buffer)) {
                                candidata = posicion;
                                tiempoHallazgo = MPI_Wtime() - tiempoInicio;
                                detener = 1;
                                break;
                            }
                        }
                        if (!detener && posicion >= finChunk) tieneChunk = 0;
                        if (!detener && cfg.tiempoMax > 0 && MPI_Wtime() - tiempoInicio > cfg.tiempoMax) {
                            agotado = 1;
                            detener = 1;
                        }
                    }
                }
            }
        }
        tiempoLocal = candidata >= 0 ? tiempoHallazgo : MPI_Wtime() - tiempoInicio;
    } else {
        long resultado = -1;
        for (;;) {
            VERIFICAR_MPI(MPI_Send(&resultado, 1, MPI_LONG, 0, ETIQUETA_SOLICITUD, MPI_COMM_WORLD));
            long asignacion;
            VERIFICAR_MPI(MPI_Recv(&asignacion, 1, MPI_LONG, 0, ETIQUETA_ASIGNACION, MPI_COMM_WORLD,
                                   MPI_STATUS_IGNORE));
            if (asignacion < 0) break;

            resultado = -1;
            long inicioChunk = asignacion * chunk;
            long finChunk = minimo(inicioChunk + chunk, limite);
            for (long intento = inicioChunk; intento < finChunk; intento++) {
                probadas++;
                if (probarLlaveBuffer(intento, cifrado, longitud, cfg.palabraClave, buffer)) {
                    resultado = intento;
                    break;
                }
            }
        }
    }

    /* Solo el maestro conoce la llave; el MPI_Allreduce de reportarResultadoMPI la difunde */
    reportarResultadoMPI("maestro_trabajador", rango, totalProcesos, 1, &cfg, candidata, agotado, tiempoLocal,
                         probadas, probadas, cifrado, longitud);
    free(buffer);
    free(cifrado);
    MPI_Finalize();
    return 0;
}
