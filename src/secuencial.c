/* Version secuencial: prueba todas las llaves 0, 1, 2, ... hasta encontrar la que descifra el texto.
 * Sirve como linea base para calcular speedups (Tsec / Tpar). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bf_common.h"
#include "des_utils.h"

static double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double) t.tv_sec + (double) t.tv_nsec * 1e-9;
}

int main(int argc, char *argv[]) {
    Config cfg;
    char error[300] = "";
    if (parsearArgumentos(argc, argv, &cfg, error, sizeof error) != 0) {
        fprintf(stderr, "Error: %s\n\n", error);
        imprimirUso(argv[0], "");
        return 1;
    }

    int longitud = 0;
    unsigned char *cifrado = leerArchivoBinario(cfg.archivoCifrado, &longitud, 1, error, sizeof error);
    if (cifrado == NULL) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }
    unsigned char *buffer = malloc((size_t) longitud + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Error: sin memoria\n");
        free(cifrado);
        return 1;
    }

    imprimirEncabezado("secuencial", 1, 1, &cfg, longitud);

    long limite = limiteDeBits(cfg.bits);
    long llaveEncontrada = -1;
    long probadas = 0;
    int agotado = 0;

    double inicio = ahora();
    for (long intento = 0; intento < limite; intento++) {
        probadas++;
        if (probarLlaveBuffer(intento, cifrado, longitud, cfg.palabraClave, buffer)) {
            llaveEncontrada = intento;
            break;
        }
        if ((intento & (INTERVALO_REVISION - 1)) == INTERVALO_REVISION - 1 && cfg.tiempoMax > 0 &&
            ahora() - inicio > cfg.tiempoMax) {
            agotado = 1;
            break;
        }
    }
    double segundos = ahora() - inicio;

    imprimirResultado("secuencial", 1, 1, &cfg, llaveEncontrada, agotado, segundos, probadas, probadas,
                      cifrado, longitud);
    free(buffer);
    free(cifrado);
    return 0;
}
