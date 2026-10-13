#include "bf_common.h"
#include "des_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void escribirError(char *msgError, size_t tam, const char *formato, const char *dato) {
    if (msgError != NULL && tam > 0) snprintf(msgError, tam, formato, dato);
}

long limiteDeBits(int bits) {
    return 1L << bits;
}

/* Convierte texto a long con validacion estricta (sin basura al final). */
static int aLong(const char *texto, long *salida) {
    char *fin = NULL;
    errno = 0;
    long valor = strtol(texto, &fin, 10);
    if (errno != 0 || fin == texto || *fin != '\0') return -1;
    *salida = valor;
    return 0;
}

static int aDouble(const char *texto, double *salida) {
    char *fin = NULL;
    errno = 0;
    double valor = strtod(texto, &fin);
    if (errno != 0 || fin == texto || *fin != '\0') return -1;
    *salida = valor;
    return 0;
}

int parsearArgumentos(int argc, char **argv, Config *cfg, char *msgError, size_t tam) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->semilla = 12345;
    int posicionales = 0;
    long bitsLeidos = 0;
    const char *textoBits = "";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            char opcion = argv[i][1];
            if (i + 1 >= argc) {
                escribirError(msgError, tam, "falta el valor de la opcion %s", argv[i]);
                return -1;
            }
            const char *valor = argv[++i];
            long entero = 0;
            double real = 0;
            switch (opcion) {
                case 't':
                    if (aDouble(valor, &real) != 0 || real < 0) {
                        escribirError(msgError, tam, "tiempo maximo invalido: %s", valor);
                        return -1;
                    }
                    cfg->tiempoMax = real;
                    break;
                case 'c':
                    if (aLong(valor, &entero) != 0 || entero < 1) {
                        escribirError(msgError, tam, "tamano de chunk invalido: %s", valor);
                        return -1;
                    }
                    cfg->chunk = entero;
                    break;
                case 's':
                    if (aLong(valor, &entero) != 0 || entero < 0) {
                        escribirError(msgError, tam, "semilla invalida: %s", valor);
                        return -1;
                    }
                    cfg->semilla = (unsigned long) entero;
                    break;
                case 'h':
                    if (aLong(valor, &entero) != 0 || entero < 1 || entero > 1024) {
                        escribirError(msgError, tam, "numero de hilos invalido: %s", valor);
                        return -1;
                    }
                    cfg->hilos = (int) entero;
                    break;
                default:
                    escribirError(msgError, tam, "opcion desconocida: %s", argv[i - 1]);
                    return -1;
            }
        } else {
            switch (posicionales) {
                case 0: cfg->archivoCifrado = argv[i]; break;
                case 1: cfg->palabraClave = argv[i]; break;
                case 2:
                    textoBits = argv[i];
                    if (aLong(argv[i], &bitsLeidos) != 0) {
                        escribirError(msgError, tam, "bits de busqueda invalido: %s", argv[i]);
                        return -1;
                    }
                    break;
                default:
                    escribirError(msgError, tam, "argumento inesperado: %s", argv[i]);
                    return -1;
            }
            posicionales++;
        }
    }

    if (posicionales < 3) {
        escribirError(msgError, tam, "%s", "faltan argumentos obligatorios");
        return -1;
    }
    if (cfg->palabraClave[0] == '\0') {
        escribirError(msgError, tam, "%s", "la palabra clave no puede estar vacia");
        return -1;
    }
    if (bitsLeidos < 1 || bitsLeidos > MAX_BITS_LLAVE) {
        escribirError(msgError, tam, "los bits de busqueda deben estar entre 1 y 56 (recibido %s)", textoBits);
        return -1;
    }
    cfg->bits = (int) bitsLeidos;
    return 0;
}

void imprimirUso(const char *programa, const char *extras) {
    fprintf(stderr, "Uso: %s <archivo_cifrado> <palabra_clave> <bits> [-t segundos]%s\n", programa,
            extras != NULL ? extras : "");
    fprintf(stderr, "  <archivo_cifrado>  archivo binario generado con des_tool cifrar\n");
    fprintf(stderr, "  <palabra_clave>    subcadena conocida del texto plano (>= %d caracteres recomendado)\n",
            MIN_LONGITUD_PALABRA);
    fprintf(stderr, "  <bits>             el espacio de busqueda es [0, 2^bits), 1 <= bits <= 56\n");
    fprintf(stderr, "  -t segundos        tiempo maximo de busqueda (por defecto sin limite)\n");
}

unsigned char *leerArchivoBinario(const char *ruta, int *longitud, int exigirMultiplo8,
                                  char *msgError, size_t tam) {
    FILE *archivo = fopen(ruta, "rb");
    if (archivo == NULL) {
        char motivo[200];
        snprintf(motivo, sizeof motivo, "no se pudo abrir '%s': %s", ruta, strerror(errno));
        escribirError(msgError, tam, "%s", motivo);
        return NULL;
    }
    if (fseek(archivo, 0, SEEK_END) != 0) {
        fclose(archivo);
        escribirError(msgError, tam, "no se pudo medir el archivo '%s'", ruta);
        return NULL;
    }
    long tamano = ftell(archivo);
    rewind(archivo);

    if (tamano <= 0) {
        fclose(archivo);
        escribirError(msgError, tam, "el archivo '%s' esta vacio", ruta);
        return NULL;
    }
    if (tamano > TAMANO_MAX_ARCHIVO) {
        fclose(archivo);
        escribirError(msgError, tam, "el archivo '%s' excede el limite de 1 MiB", ruta);
        return NULL;
    }
    if (exigirMultiplo8 && tamano % 8 != 0) {
        fclose(archivo);
        escribirError(msgError, tam, "'%s' no es un archivo cifrado valido (tamano no multiplo de 8)", ruta);
        return NULL;
    }

    unsigned char *datos = malloc((size_t) tamano);
    if (datos == NULL) {
        fclose(archivo);
        escribirError(msgError, tam, "%s", "sin memoria suficiente para leer el archivo");
        return NULL;
    }
    if (fread(datos, 1, (size_t) tamano, archivo) != (size_t) tamano) {
        free(datos);
        fclose(archivo);
        escribirError(msgError, tam, "error al leer '%s'", ruta);
        return NULL;
    }
    fclose(archivo);
    *longitud = (int) tamano;
    return datos;
}

void imprimirEncabezado(const char *algoritmo, int procesos, int hilos, const Config *cfg, int longitud) {
    printf("===== Busqueda por fuerza bruta (DES) =====\n");
    printf("Algoritmo:         %s\n", algoritmo);
    printf("Archivo cifrado:   %s (%d bytes)\n", cfg->archivoCifrado, longitud);
    printf("Palabra clave:     \"%s\"\n", cfg->palabraClave);
    printf("Espacio de busqueda: 2^%d = %ld llaves\n", cfg->bits, limiteDeBits(cfg->bits));
    printf("Procesos MPI:      %d   Hilos OpenMP/proceso: %d\n", procesos, hilos);
    if (cfg->tiempoMax > 0) printf("Tiempo maximo:     %.1f s\n", cfg->tiempoMax);
    if ((int) strlen(cfg->palabraClave) < MIN_LONGITUD_PALABRA) {
        printf("ADVERTENCIA: palabra clave corta (< %d caracteres): riesgo de falsos positivos.\n",
               MIN_LONGITUD_PALABRA);
    }
    fflush(stdout);
}

void imprimirResultado(const char *algoritmo, int procesos, int hilos, const Config *cfg,
                       long llave, int agotado, double segundos, long iterMax, long iterTotal,
                       const unsigned char *cifrado, int longitud) {
    const char *estado = llave >= 0 ? "encontrada" : (agotado ? "tiempo_agotado" : "no_encontrada");

    if (llave >= 0) {
        printf("Llave encontrada:  %ld (0x%lX)\n", llave, llave);
        unsigned char *copia = malloc((size_t) longitud + 1);
        if (copia != NULL) {
            memcpy(copia, cifrado, longitud);
            copia[longitud] = '\0';
            descifrarBloque(llave, copia, longitud);
            printf("Texto descifrado:  \"");
            int largoTexto = 0;
            while (largoTexto < longitud && copia[largoTexto] != '\0') largoTexto++;
            while (largoTexto > 0 && (copia[largoTexto - 1] == '\n' || copia[largoTexto - 1] == '\r')) largoTexto--;
            for (int i = 0; i < largoTexto && i < 200; i++) {
                putchar((copia[i] >= 32 && copia[i] < 127) ? copia[i] : '?');
            }
            printf("\"\n");
            printf("Verificacion:      %s\n",
                   strstr((char *) copia, cfg->palabraClave) != NULL ? "OK (contiene la palabra clave)"
                                                                      : "FALLO");
            free(copia);
        }
    } else if (agotado) {
        printf("Llave NO encontrada: se alcanzo el tiempo maximo (%.1f s)\n", cfg->tiempoMax);
    } else {
        printf("Llave NO encontrada en el espacio dado\n");
    }
    printf("Tiempo de busqueda: %.4f s\n", segundos);
    printf("Iteraciones: max por trabajador = %ld, total = %ld\n", iterMax, iterTotal);
    /* Linea legible por maquina para la bitacora (scripts/benchmark.sh) */
    printf("CSV:%s,%d,%d,%d,%ld,%s,%.6f,%ld,%ld\n", algoritmo, procesos, hilos, cfg->bits, llave, estado,
           segundos, iterMax, iterTotal);
    fflush(stdout);
}
