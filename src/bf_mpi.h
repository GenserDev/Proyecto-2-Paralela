#ifndef BF_MPI_H
#define BF_MPI_H

#include <mpi.h>
#include "bf_common.h"

/* Verifica el codigo de retorno de una llamada MPI; si falla, imprime el error y aborta todo. */
#define VERIFICAR_MPI(llamada)                                                        \
    do {                                                                              \
        int rc_ = (llamada);                                                          \
        if (rc_ != MPI_SUCCESS) {                                                     \
            char msg_[MPI_MAX_ERROR_STRING];                                          \
            int len_ = 0;                                                             \
            MPI_Error_string(rc_, msg_, &len_);                                       \
            fprintf(stderr, "Error MPI en %s:%d (%s): %s\n", __FILE__, __LINE__,      \
                    #llamada, msg_);                                                  \
            MPI_Abort(MPI_COMM_WORLD, rc_);                                           \
        }                                                                             \
    } while (0)

/* Valida argumentos en todos los rangos, lee el archivo cifrado en el rango 0 y lo difunde
 * (asi los nodos de un cluster no necesitan el archivo). Devuelve 0 si todo salio bien o -1
 * si hubo un error ya reportado; en ese caso el llamador solo debe llamar MPI_Finalize. */
int prepararEjecucion(int argc, char **argv, int rango, const char *programa, const char *usoExtra,
                      Config *cfg, unsigned char **cifrado, int *longitud);

/* Combina los resultados locales de todos los rangos y los imprime en el rango 0.
 *   candidata: llave encontrada por este rango (-1 si no encontro)
 *   iterLocal: llaves probadas por el trabajador mas cargado de este rango
 *   iterSumaLocal: llaves probadas en total por este rango */
void reportarResultadoMPI(const char *algoritmo, int rango, int procesos, int hilos, const Config *cfg,
                          long candidata, int agotadoLocal, double segundosLocal, long iterMaxLocal,
                          long iterSumaLocal, const unsigned char *cifrado, int longitud);

#endif
