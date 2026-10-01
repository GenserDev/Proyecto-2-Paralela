# Proyecto 2, Parte A: fuerza bruta sobre DES con MPI

Computacion Paralela y Distribuida, Universidad del Valle de Guatemala, semestre 2 de 2024.

Esta parte del proyecto investiga el algoritmo DES y prepara la base del programa que en la
Parte B se usara para romper por fuerza bruta la llave con la que se cifro un texto. Incluye una
version secuencial que mide el tiempo de busqueda segun la longitud de la llave, y una version
paralela con Open MPI que reparte el rango de busqueda entre varios procesos.

El informe completo con la investigacion de DES, el diagrama de flujo y la explicacion de cada
rutina esta en [informe/parte_a.md](informe/parte_a.md).

## Como correr el proyecto

**1. Instalar las dependencias**

En macOS con Homebrew:

brew install open-mpi openssl@3

**2. Compilar los programas**

make

**3. Ejecutar la version secuencial**

La version secuencial recibe la cantidad de bits del rango de busqueda y la llave real con la que
se cifra el texto de prueba.

./secuencial 20 12345

**4. Ejecutar la version paralela**

La version paralela recibe la llave real y el limite superior del rango de busqueda. Se puede
ajustar el numero de procesos con `-np`.

mpirun -np 4 ./bruteforce_mpi 123456 2000000

## Notas

El programa base del enunciado usa la libreria `rpc/des_crypt.h`, que ya no viene incluida en
macOS ni en distribuciones recientes de Linux. En su lugar se usan las funciones DES de OpenSSL,
como el mismo enunciado permite.

El Makefile busca OpenSSL en la ruta que reporta `brew --prefix openssl@3`. Si se usa otro sistema
o gestor de paquetes, hay que ajustar esa ruta en el Makefile.
