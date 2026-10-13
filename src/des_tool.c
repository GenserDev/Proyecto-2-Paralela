/* des_tool: cifra o descifra un archivo con una llave DES arbitraria (numero entero).
 *   ./des_tool cifrar    <llave> <entrada.txt> <salida.cif>
 *   ./des_tool descifrar <llave> <entrada.cif> <salida.txt>
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf_common.h"
#include "des_utils.h"

static int escribirArchivo(const char *ruta, const unsigned char *datos, size_t largo) {
    FILE *f = fopen(ruta, "wb");
    if (f == NULL) {
        fprintf(stderr, "Error: no se pudo crear '%s': %s\n", ruta, strerror(errno));
        return -1;
    }
    int ok = fwrite(datos, 1, largo, f) == largo;
    ok = (fclose(f) == 0) && ok;
    if (!ok) fprintf(stderr, "Error: fallo al escribir '%s'\n", ruta);
    return ok ? 0 : -1;
}

static int leerLlave(const char *texto, long *llave) {
    char *fin = NULL;
    errno = 0;
    long valor = strtol(texto, &fin, 10);
    if (errno != 0 || fin == texto || *fin != '\0' || valor < 0 || valor >= limiteDeBits(MAX_BITS_LLAVE)) {
        fprintf(stderr, "Error: la llave debe ser un entero en [0, 2^56): %s\n", texto);
        return -1;
    }
    *llave = valor;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5 || (strcmp(argv[1], "cifrar") != 0 && strcmp(argv[1], "descifrar") != 0)) {
        fprintf(stderr, "Uso: %s cifrar    <llave> <entrada.txt> <salida.cif>\n", argv[0]);
        fprintf(stderr, "     %s descifrar <llave> <entrada.cif> <salida.txt>\n", argv[0]);
        return 1;
    }
    int cifrar = strcmp(argv[1], "cifrar") == 0;
    long llave;
    if (leerLlave(argv[2], &llave) != 0) return 1;

    char error[300] = "";
    int largo = 0;
    unsigned char *datos = leerArchivoBinario(argv[3], &largo, !cifrar, error, sizeof error);
    if (datos == NULL) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    int estado = 0;
    if (cifrar) {
        if (memchr(datos, 0, (size_t) largo) != NULL) {
            fprintf(stderr, "Error: el texto de entrada contiene bytes nulos\n");
            free(datos);
            return 1;
        }
        /* Relleno con ceros hasta multiplo de 8 (siempre al menos 1 byte, como en la base) */
        int largoCifrado = (largo / 8 + 1) * 8;
        unsigned char *bloque = calloc((size_t) largoCifrado, 1);
        if (bloque == NULL) {
            fprintf(stderr, "Error: sin memoria\n");
            free(datos);
            return 1;
        }
        memcpy(bloque, datos, (size_t) largo);
        cifrarBloque(llave, bloque, largoCifrado);
        estado = escribirArchivo(argv[4], bloque, (size_t) largoCifrado);
        if (estado == 0) {
            printf("Texto de %d bytes cifrado con la llave %ld -> %s (%d bytes)\n", largo, llave, argv[4],
                   largoCifrado);
        }
        free(bloque);
    } else {
        descifrarBloque(llave, datos, largo);
        int util = largo;
        while (util > 0 && datos[util - 1] == 0) util--; /* quitar relleno */
        estado = escribirArchivo(argv[4], datos, (size_t) util);
        if (estado == 0) {
            printf("Archivo descifrado con la llave %ld -> %s\nContenido: ", llave, argv[4]);
            for (int i = 0; i < util && i < 300; i++) putchar((datos[i] >= 32 && datos[i] < 127) || datos[i] == '\n' ? datos[i] : '?');
            putchar('\n');
        }
    }
    free(datos);
    return estado == 0 ? 0 : 1;
}
