#ifndef DES_UTILS_H
#define DES_UTILS_H

void llaveDesdeNumero(long numero, unsigned char llave[8]);
void cifrarBloque(long numero, unsigned char *datos, int longitud);
void descifrarBloque(long numero, unsigned char *datos, int longitud);
int probarLlave(long numero, const unsigned char *cifrado, int longitud, const char *palabraClave);

#endif
