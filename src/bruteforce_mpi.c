#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "des_utils.h"

#define ETIQUETA_ENCONTRADA 100

int main(int argc, char *argv[]) {
    int rango, totalProcesos;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rango);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesos);

    if (argc < 3) {
        if (rango == 0) {
            printf("Uso: mpirun -np <procesos> %s <llave_real> <limite_superior>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    long llaveReal = atol(argv[1]);
    long limiteSuperior = atol(argv[2]);

    const char *textoPlano = "Esta es una prueba de proyecto 2";
    const char *palabraClave = "es una prueba de";

    int longitud = ((int) strlen(textoPlano) / 8 + 1) * 8;
    unsigned char cifrado[longitud];
    memset(cifrado, 0, longitud);
    memcpy(cifrado, textoPlano, strlen(textoPlano));
    cifrarBloque(llaveReal, cifrado, longitud);

    long tamanoSegmento = limiteSuperior / totalProcesos;
    long inicioSegmento = rango * tamanoSegmento;
    long finSegmento = (rango == totalProcesos - 1) ? limiteSuperior : inicioSegmento + tamanoSegmento;

    long avisoEncontrada = -1;
    int senalRecibida = 0;
    MPI_Request solicitudRecepcion;
    MPI_Irecv(&avisoEncontrada, 1, MPI_LONG, MPI_ANY_SOURCE, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD, &solicitudRecepcion);

    double tiempoInicio = MPI_Wtime();

    long llaveEncontrada = -1;
    for (long intento = inicioSegmento; intento < finSegmento && !senalRecibida; intento++) {
        if (probarLlave(intento, cifrado, longitud, palabraClave)) {
            llaveEncontrada = intento;
            for (int destino = 0; destino < totalProcesos; destino++) {
                MPI_Send(&intento, 1, MPI_LONG, destino, ETIQUETA_ENCONTRADA, MPI_COMM_WORLD);
            }
            break;
        }
        MPI_Test(&solicitudRecepcion, &senalRecibida, MPI_STATUS_IGNORE);
    }

    if (llaveEncontrada == -1) {
        MPI_Wait(&solicitudRecepcion, MPI_STATUS_IGNORE);
        llaveEncontrada = avisoEncontrada;
    }

    double tiempoFin = MPI_Wtime();

    if (rango == 0) {
        printf("Llave encontrada: %ld\n", llaveEncontrada);
        printf("Procesos usados: %d\n", totalProcesos);
        printf("Tiempo total: %.4f segundos\n", tiempoFin - tiempoInicio);
    }

    MPI_Finalize();
    return 0;
}
