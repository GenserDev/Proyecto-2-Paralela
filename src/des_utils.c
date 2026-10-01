#include "des_utils.h"
#include <openssl/des.h>
#include <string.h>

void llaveDesdeNumero(long numero, unsigned char llave[8]) {
    for (int i = 0; i < 8; i++) {
        unsigned char septeto = (numero >> (i * 7)) & 0x7F;
        llave[i] = septeto << 1;
    }
    DES_set_odd_parity((DES_cblock *) llave);
}

static void procesarBloques(long numero, unsigned char *datos, int longitud, int modoCifrar) {
    unsigned char llave[8];
    llaveDesdeNumero(numero, llave);

    DES_key_schedule programacionLlave;
    // unchecked porque revisar llaves debiles en cada intento ralentiza la busqueda por fuerza bruta
    DES_set_key_unchecked((DES_cblock *) llave, &programacionLlave);

    for (int inicio = 0; inicio < longitud; inicio += 8) {
        DES_ecb_encrypt((DES_cblock *) (datos + inicio), (DES_cblock *) (datos + inicio),
                         &programacionLlave, modoCifrar);
    }
}

void cifrarBloque(long numero, unsigned char *datos, int longitud) {
    procesarBloques(numero, datos, longitud, DES_ENCRYPT);
}

void descifrarBloque(long numero, unsigned char *datos, int longitud) {
    procesarBloques(numero, datos, longitud, DES_DECRYPT);
}

int probarLlave(long numero, const unsigned char *cifrado, int longitud, const char *palabraClave) {
    unsigned char copia[longitud + 1];
    memcpy(copia, cifrado, longitud);
    copia[longitud] = '\0';

    descifrarBloque(numero, copia, longitud);

    return strstr((char *) copia, palabraClave) != NULL;
}
