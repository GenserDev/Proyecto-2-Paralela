#ifndef DES_UTILS_H
#define DES_UTILS_H

/* Convierte un numero de llave (56 bits utiles) en una llave DES de 8 bytes. */
void llaveDesdeNumero(long numero, unsigned char llave[8]);

/* Cifra/descifra "datos" in situ (modo ECB). "longitud" debe ser multiplo de 8. */
void cifrarBloque(long numero, unsigned char *datos, int longitud);
void descifrarBloque(long numero, unsigned char *datos, int longitud);

/* Devuelve 1 si descifrar "cifrado" con la llave "numero" produce un texto que contiene
 * "palabraClave". Version simple: reserva memoria en cada llamada (no usar en ciclos calientes). */
int probarLlave(long numero, const unsigned char *cifrado, int longitud, const char *palabraClave);

/* Igual que probarLlave, pero usa un buffer provisto por el llamador de al menos
 * longitud + 1 bytes (sin malloc ni arreglos de tamano variable en la pila). */
int probarLlaveBuffer(long numero, const unsigned char *cifrado, int longitud,
                      const char *palabraClave, unsigned char *buffer);

#endif
