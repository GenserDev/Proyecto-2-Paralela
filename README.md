# Proyecto 2: fuerza bruta sobre DES con MPI

Computación Paralela y Distribuida, Universidad del Valle de Guatemala, semestre 2 de 2024.

Programas que encuentran por fuerza bruta la llave DES con la que se cifró un texto, probando todas las
llaves de un rango hasta que una descifra un texto que contiene una palabra clave conocida. Incluye una
versión secuencial, la versión paralela "naive" con Open MPI, dos acercamientos alternativos
(entrelazado y maestro-trabajador), y extras (offset aleatorio e híbrido MPI + OpenMP). Descripción y
pseudocódigo de cada acercamiento en [docs/acercamientos.md](docs/acercamientos.md).

## Dependencias

macOS (Homebrew):

    brew install open-mpi openssl@3 libomp

Linux (Debian/Ubuntu):

    sudo apt install build-essential libopenmpi-dev openmpi-bin libssl-dev

El programa base del enunciado usa `rpc/des_crypt.h`, que ya no viene en macOS ni en Linux recientes. Se usan
las funciones DES de OpenSSL (como permite el enunciado). `libomp` solo hace falta para la versión híbrida.

## Compilar

    make            # todo menos la versión híbrida
    make hybrid     # versión MPI + OpenMP
    make todo       # todo

En macOS el Makefile busca OpenSSL y libomp con `brew --prefix`. Si `mpicc` envuelve un `gcc` de Homebrew en
lugar de clang, usar `make hybrid OMP_CFLAGS=-fopenmp OMP_LDFLAGS=-fopenmp`.

## Paso 1: cifrar y descifrar un texto desde un archivo

`des_tool` cifra/descifra un `.txt` con una llave arbitraria (entero en `[0, 2^56)`):

    echo "Esta es una prueba de proyecto 2" > datos/texto.txt
    ./des_tool cifrar    42 datos/texto.txt     datos/texto_42.cif
    ./des_tool descifrar 42 datos/texto_42.cif  datos/recuperado.txt

## Paso 2: buscar la llave

Todos los programas reciben lo mismo: archivo cifrado, palabra clave y los **bits** del espacio de búsqueda
(se prueba `[0, 2^bits)`, `1 ≤ bits ≤ 56`):

    ./secuencial datos/texto_42.cif "es una prueba de" 20

    mpirun -np 4 ./bruteforce_mpi          datos/texto_42.cif "es una prueba de" 20   # naive
    mpirun -np 4 ./bruteforce_interleaved  datos/texto_42.cif "es una prueba de" 20   # acercamiento 1
    mpirun -np 4 ./bruteforce_masterworker datos/texto_42.cif "es una prueba de" 20   # acercamiento 2
    mpirun -np 4 ./bruteforce_randoffset   datos/texto_42.cif "es una prueba de" 20   # extra: offset aleatorio
    mpirun -np 2 ./bruteforce_hybrid       datos/texto_42.cif "es una prueba de" 20 -h 2   # extra: MPI + OpenMP

Opciones comunes: `-t <segundos>` tiempo máximo (necesario para espacios de 56 bits, que no terminan);
`-c <llaves>` tamaño de chunk (maestro-trabajador); `-s <semilla>` (offset aleatorio); `-h <hilos>` (híbrido).
Cada programa imprime la llave encontrada, el nombre del archivo cifrado, la palabra clave, el texto
descifrado como verificación, el tiempo y las iteraciones, y una línea `CSV:` para la bitácora.

Programación defensiva: se valida el número de argumentos, que el archivo exista y sea un cifrado válido
(tamaño múltiplo de 8, ≤ 1 MiB), que la palabra clave no esté vacía (advierte si tiene < 8 caracteres) y que
`bits` esté en `[1, 56]`. Si la llave no está en el rango, todos los procesos terminan (no hay bloqueo).

## Pruebas del enunciado (bitácora)

Con 56 bits reales solo terminan las llaves que el naive encuentra de inmediato (`123456`, `2^54`, `2^55+1`);
las demás tardarían años. Por eso `scripts/benchmark.sh` usa las mismas llaves **escaladas** al espacio
`2^BITS` (las fracciones del enunciado: `S/4−1`, `S/4`, `S/2+1`, `S/2+S/8`, `S/7+S/13`):

    scripts/benchmark.sh 26 4 5      # 2^26 llaves, -np 4, 5 repeticiones por combinación

Genera `resultados/bitacora.csv` (una fila por corrida) y un resumen en Markdown con tiempos, iteraciones y
speedups por llave y por algoritmo. El **speedup en iteraciones** (`iter_secuencial / iter_max_paralelo`) no
depende del hardware y es el que corresponde al análisis "Tpar = 1 iter" del enunciado; el de tiempo
depende de la máquina.

Para reproducir las pruebas con 56 bits reales (por ejemplo la llave `…983`, que no termina):

    ./des_tool cifrar 18014398509481983 datos/texto.txt datos/k_b.cif
    mpirun -np 4 ./bruteforce_mpi datos/k_b.cif "es una prueba de" 56 -t 60

## Extra: cluster MPI con 2 o más máquinas

1. **Mismo entorno:** mismo sistema operativo/arquitectura y misma versión de Open MPI en todas las máquinas
   (`mpirun --version`), en la misma red local (sin virtualizar).
2. **SSH sin contraseña** desde la máquina que lanza `mpirun` hacia las demás:
   `ssh-keygen -t ed25519` y `ssh-copy-id usuario@maquina2`. Verificar con `ssh maquina2 hostname`.
3. **Mismo directorio con los ejecutables** en todas las máquinas (clonar el repo y hacer `make` en la misma
   ruta, o NFS). El archivo cifrado solo tiene que existir donde se lanza `mpirun`: el rango 0 lo lee y lo
   difunde por MPI.
4. **Archivo de hosts** (`hosts`), con los núcleos de cada máquina:

       maquina1 slots=4
       maquina2 slots=4

5. **Ejecutar:**

       mpirun --hostfile hosts -np 8 ./bruteforce_masterworker datos/texto_42.cif "es una prueba de" 28
       mpirun --hostfile hosts -np 4 ./bruteforce_hybrid datos/texto_42.cif "es una prueba de" 28 -h 4

   El benchmark también acepta el cluster: `HOSTFILE=hosts scripts/benchmark.sh 28 8 3`.

Si hay varias interfaces de red, indicar la correcta con `--mca btl_tcp_if_include <interfaz>`; si hay
firewall, permitir las conexiones entre las máquinas. Guía base: https://mpitutorial.com/tutorials/running-an-mpi-cluster-within-a-lan/

## Estructura

    src/des_utils.[ch]       cifrado/descifrado DES y prueba de una llave (OpenSSL)
    src/bf_common.[ch]       argumentos, lectura de archivos, impresión de resultados
    src/bf_mpi.[ch]          preparación común MPI (lectura + difusión del cifrado) y reducción de resultados
    src/des_tool.c           cifrar/descifrar archivos con una llave
    src/secuencial.c         versión secuencial
    src/bruteforce_mpi.c     naive (bloques contiguos)
    src/bruteforce_interleaved.c, bruteforce_masterworker.c   acercamientos alternativos 1 y 2
    src/bruteforce_randoffset.c, bruteforce_hybrid.c          extras
    scripts/benchmark.sh, scripts/resumen.py                   bitácora y resumen de speedups
    docs/acercamientos.md    descripción, pseudocódigo y análisis de cada acercamiento

## Notas

- El tiempo reportado es el del trabajador más lento hasta que la llave se conoce (`MPI_Wtime` después de un
  `MPI_Barrier`), no incluye lectura del archivo ni arranque de MPI.
- Los programas revisan la señal de "llave encontrada" cada 4096 llaves (`INTERVALO_REVISION`), igual en
  todos los acercamientos, para que la comparación mida el reparto del espacio y no el costo de `MPI_Test`.
- La CLI de `secuencial` cambió respecto a la Parte A (`./secuencial 20 12345`): ahora recibe archivo cifrado,
  palabra clave y bits, igual que las versiones paralelas.
