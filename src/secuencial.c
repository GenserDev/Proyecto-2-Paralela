#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "des_utils.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <bits_de_busqueda> <llave_real>\n", argv[0]);
        return 1;
    }

    int bits = atoi(argv[1]);
    long llaveReal = atol(argv[2]);

    const char *textoPlano = "Esta es una prueba de fuerza bruta";
    const char *palabraClave = "prueba de fuerza";

    int longitud = ((int) strlen(textoPlano) / 8 + 1) * 8;
    unsigned char cifrado[longitud];
    memset(cifrado, 0, longitud);
    memcpy(cifrado, textoPlano, strlen(textoPlano));
    cifrarBloque(llaveReal, cifrado, longitud);

    long limite = 1L << bits;

    clock_t inicio = clock();

    long llaveEncontrada = -1;
    for (long intento = 0; intento < limite; intento++) {
        if (probarLlave(intento, cifrado, longitud, palabraClave)) {
            llaveEncontrada = intento;
            break;
        }
    }

    double segundos = (double) (clock() - inicio) / CLOCKS_PER_SEC;

    if (llaveEncontrada >= 0) {
        printf("Llave encontrada: %ld\n", llaveEncontrada);
    } else {
        printf("No se encontro la llave en el rango dado\n");
    }
    printf("Bits de busqueda: %d\n", bits);
    printf("Tiempo de busqueda: %.4f segundos\n", segundos);

    return 0;
}
