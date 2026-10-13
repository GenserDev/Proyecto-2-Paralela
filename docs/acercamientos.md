# Acercamientos para repartir el espacio de búsqueda

Notación: espacio de llaves `[0, S)` con `S = 2^bits`, `P` procesos MPI, `T` hilos OpenMP por proceso,
`k` = llave real, `B = S/P` = tamaño de bloque. `Tpar` se mide en **iteraciones** del trabajador más cargado
(cuántas llaves prueba antes de que se conozca la llave), que es la medida que usa el enunciado
("Tpar = 1 iter"). El speedup en iteraciones es `Tsec / Tpar` con `Tsec = k + 1`.

Todos los acercamientos comparten la misma condición de parada: el que encuentra la llave la avisa con
`MPI_Send` a los demás; los demás tienen una `MPI_Irecv` pendiente que revisan con `MPI_Test` cada
`INTERVALO_REVISION` (4096) llaves. Al final todos entran a un `MPI_Allreduce`, así que el programa termina
aunque la llave no esté en el rango.

---

## 0. Naive (`bruteforce_mpi`)

Bloques contiguos de tamaño `B`; el proceso `r` recorre `[r·B, (r+1)·B)` en orden.

```
para intento en [r*B, (r+1)*B):
    si probarLlave(intento): avisar a todos; terminar
    cada 4096 iteraciones: si llegó aviso: terminar
```

- `Tpar = (k mod B) + 1`  →  `speedup = (k+1) / ((k mod B) + 1)`.
- Si `k` cae al inicio de un bloque ≠ 0, el speedup es enorme (superlineal, engañoso).
- Si `k` cae al final del bloque 0, el speedup es 1: no hay ninguna ganancia.
- El resultado depende del valor de la llave, no del algoritmo.

## 1. Entrelazado / round-robin (`bruteforce_interleaved`)

El proceso `r` prueba `r, r+P, r+2P, …` (todo el espacio con salto `P`).

```
para intento = r; intento < S; intento += P:
    si probarLlave(intento): avisar a todos; terminar
    cada 4096 iteraciones: si llegó aviso: terminar
```

- Todos los procesos avanzan sobre el mismo "frente" 0, 1, 2, …, así que la llave `k` se encuentra tras
  `Tpar = ⌊k/P⌋ + 1` iteraciones, sin importar cuál sea `k`.
- `speedup ≈ P` **para cualquier llave** (consistente). Pierde los "speedups gigantes" del naive, pero
  también elimina el peor caso (speedup 1).
- Costo extra: ninguno (misma cantidad de trabajo por iteración).

## 2. Maestro-trabajador con chunks dinámicos (`bruteforce_masterworker`)

El espacio se corta en chunks consecutivos de `C` llaves. El maestro (rango 0) los reparte bajo demanda en
orden creciente y además trabaja él mismo entre atenciones.

```
maestro:                                   trabajador:
  mientras haya trabajadores o trabajo:      repetir:
    atender solicitudes pendientes:            enviar(resultado_anterior)       # -1 si nada
      si resultado >= 0: detener = 1           chunk = recibir()
      responder chunk siguiente o -1 (parar)   si chunk < 0: terminar
    probar una rebanada de mi chunk propio     probar todas las llaves del chunk
```

- El frente de búsqueda sigue siendo compacto (los chunks salen en orden), así que
  `Tpar ≈ k/P + O(C)` iteraciones: speedup ≈ P para cualquier llave.
- Balancea carga automáticamente si los procesos van a velocidades distintas (útil en un cluster
  heterogéneo): quien termina antes pide más trabajo.
- Costo: mensajes de control (uno por chunk) y un retraso de hasta un chunk al detenerse. Con chunks
  de ≥ 1024 llaves (~ms) el costo de comunicación es despreciable.
- Parámetro `-c` fija el tamaño de chunk (por defecto ≈ `S/(256·P)` acotado a `[1024, 2^20]`).

## 3. Offset aleatorio (`bruteforce_randoffset`) — extra

Bloques contiguos como el naive, pero cada proceso empieza en una posición aleatoria de su bloque y lo
recorre de forma circular (semilla `-s` para reproducibilidad).

- En esperanza el tiempo de un proceso es `B/2` sin importar `k`, así que rompe el peor caso fijo del naive.
- **Limitación:** pierde la ventaja de las llaves pequeñas. Si `k ≪ B`, el entrelazado la encuentra en
  `k/P` iteraciones pero el offset aleatorio tarda `~B/2` en promedio, que puede ser peor que el secuencial.
  Es un buen contraejemplo para la discusión del informe.

## 4. Híbrido MPI + OpenMP (`bruteforce_hybrid`) — extra

Entrelazado con `W = P·T` trabajadores: el hilo `t` del proceso `r` prueba `w, w+W, w+2W, …` con `w = r·T + t`.

- Dentro de un proceso, los hilos se avisan con una bandera atómica (memoria compartida).
- Entre procesos se usa el mismo `MPI_Send` / `MPI_Irecv` (memoria distribuida).
- Se inicia con `MPI_THREAD_FUNNELED`: solo el hilo 0 llama a MPI dentro de la región paralela; el envío
  del aviso lo hace el hilo principal al salir de ella.
- Cada hilo usa su propio buffer de descifrado (sin estado compartido en el ciclo caliente).
- Para el extra: comparar `-np 4 -h 1` (4 procesos) contra `-np 2 -h 2` y `-np 1 -h 4` (mismos 4 núcleos)
  y reportar el speedup frente a la versión sin OpenMP.

---

## Resumen del "tiempo paralelo esperado"

| Acercamiento | Tpar (iteraciones) | Speedup | Peor caso |
|---|---|---|---|
| Naive | `(k mod B) + 1` | de 1 a superlineal, según `k` | speedup 1 |
| Entrelazado | `⌊k/P⌋ + 1` | ≈ P siempre | ≈ P |
| Maestro-trabajador | `≈ k/P + O(C)` | ≈ P siempre | ≈ P (menos overhead) |
| Offset aleatorio | `≈ B/2` en esperanza | muy variable | puede ser < 1 si `k ≪ B` |
| Híbrido | `⌊k/(P·T)⌋ + 1` | ≈ P·T | ≈ P·T |

Nota sobre el enunciado: las llaves que da el PDF son `S/4 − 1` (`…983`, última llave del bloque 0 con 4
procesos) y `S/4` (`…984`, primera del bloque 1) con `S = 2^56`; coinciden con el análisis de la discusión
aunque el texto escriba `2^56/4` para la primera.
