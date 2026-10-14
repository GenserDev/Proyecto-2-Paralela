# Análisis de resultados — Parte B

Datos: `bitacora.csv` generada con `scripts/benchmark.sh 26 4 5` (espacio `S = 2^26`, `-np 4`,
5 repeticiones por combinación, 180 corridas; todas encontraron la llave). El híbrido corrió con
`-np 4 -h 2` (8 trabajadores), por lo que sus columnas **no son comparables** con las de 4 procesos
(ver la comparación controlada en la sección Extras). La velocidad secuencial fue de ~444 ns por llave, estable entre
llaves (CV del tiempo secuencial ≈ 2–3 %).

Llaves usadas (las del enunciado escaladas con `S = 2^26`):

| Id | Llave | Posición | Bloque naive (4 procesos) |
|---|---|---|---|
| a | 123456 | 0.2 % de S | 0, cerca del inicio |
| b | S/4 − 1 | 25 % − 1 | 0, **última** posición |
| c | S/4 | 25 % | 1, **primera** posición |
| d (fácil) | S/2 + 1 | 50 % + 1 | 2, segunda posición |
| e (media) | S/2 + S/8 | 62.5 % | 2, a la mitad |
| f (difícil) | ⌈S/7⌉ + ⌈S/13⌉ | ≈ 22 % | 0, al 88 % del bloque |

## Speedup en iteraciones (independiente del hardware)

`Tsec / Tpar`, con `Tpar` = llaves probadas por el trabajador más cargado.

| Llave | Naive | Entrelazado | Maestro-trabajador | Offset aleatorio | Híbrido 4×2 |
|---|---|---|---|---|---|
| a 123456 | 0.97 | 3.77 | 1.88 | 0.04 | 4.57 |
| b S/4−1 | 1.00 | 3.99 | 3.84 | 5.43 | 7.20 |
| c S/4 | **4096.00** | 3.98 | 3.89 | 1.25 | 7.07 |
| d S/2+1 (fácil) | **8192.00** | 3.99 | 3.88 | 2.40 | 7.26 |
| e S/2+S/8 (media) | 4.99 | 4.00 | 3.88 | 7.50 | 6.86 |
| f S/7+S/13 (difícil) | 1.00 | 4.00 | 3.90 | 14.02 | 7.77 |

## Speedup en tiempo (`Tsec / Tpar`, tiempos promedio)

| Llave | T sec (s) | Naive | Entrelazado | Maestro-trabajador | Offset aleatorio | Híbrido 4×2 |
|---|---|---|---|---|---|---|
| a 123456 | 0.056 | 0.90 | 3.36 | 1.83 | 0.04 | 3.78 |
| b S/4−1 | 7.64 | 0.95 | 3.88 | 3.64 | 5.19 | 4.98 |
| c S/4 | 7.32 | **2906** | 3.62 | 3.56 | 1.15 | 4.89 |
| d S/2+1 | 15.27 | **7730** | 3.73 | 3.74 | 2.21 | 5.04 |
| e S/2+S/8 | 18.50 | 4.77 | 3.80 | 3.66 | 6.32 | 4.85 |
| f S/7+S/13 | 6.37 | 0.94 | 3.77 | 3.68 | 13.35 | 5.57 |

Gráfica: `speedups_parteB.png` (escala logarítmica; la línea punteada es el ideal de 4×).

---

## 2d. Reflexión: el tiempo del naive en función de la llave

El tiempo del naive **no depende de qué tan grande es la llave, sino de en qué posición de su bloque cae**.
Con 4 procesos, el proceso que tiene la llave la encuentra tras `(k mod B) + 1` iteraciones, y `B = S/4`:

- **b (`S/4 − 1`)**: última posición del bloque 0 → 16 777 216 iteraciones, igual que el secuencial
  (speedup 1.00; 8.04 s vs 7.64 s en tiempo, la diferencia es el costo de MPI y de ejecutar 4 procesos).
  Es el peor caso: 4 procesos hacen el trabajo de 1.
- **c (`S/4`)**: primera posición del bloque 1 → el proceso 1 la encuentra en la primera revisión. En el
  programa, `iter_max = 4096` porque la señal se revisa cada 4096 llaves, y el resto de procesos ya habían
  empezado. Speedup de 4096× en iteraciones y 2906× en tiempo.
- **d (`S/2 + 1`)**: segunda llave del bloque 2 → speedup 8192× en iteraciones. Mismo fenómeno.
- **e y f** quedan en medio: e cae a la mitad del bloque 2 (speedup 4.99, casi lineal por casualidad) y f
  cae al 88 % del bloque 0 (speedup 1.00).

Una diferencia de **una sola unidad** en la llave (`b` vs `c`) cambia el tiempo de 8.04 s a 0.0025 s, un
factor de ~3000. Esto confirma lo que dice el enunciado: los speedups del naive son "sumamente
inconsistentes y dependientes de la llave". Además, los speedups gigantes (c, d) **no indican que el algoritmo
sea bueno**: son un artefacto de que la llave quedó al inicio de un bloque. El tiempo esperado sobre
llaves uniformes no mejora respecto a un reparto mejor; solo cambia la varianza.

## 5. Comparación de acercamientos (llaves fácil / media / difícil)

**Entrelazado** — speedup en iteraciones entre 3.77 y 4.00 para las seis llaves; en tiempo, entre 3.36 y
3.88 (eficiencia media 94 %, rango 91–97 %). La llave `k` se encuentra tras `⌊k/P⌋ + 1` iteraciones sin
importar su valor: el tiempo es proporcional a `k`, no a su posición en un bloque. Es el acercamiento más
consistente y el de menor overhead.

**Maestro-trabajador** — 3.84–3.90 en iteraciones para las llaves grandes y 3.56–3.74 en tiempo (eficiencia
91 %, algo menor que el entrelazado por los mensajes de control y por la rebanada que el maestro prueba
antes de atender solicitudes). Para la llave pequeña `a` el speedup baja a 1.88 porque el chunk
automático es de 65 536 llaves: con `k = 123 456`, `k` cae en el chunk 1 y el trabajo útil lo hace un
solo trabajador durante ~58 000 iteraciones. Es una limitación de granularidad: `Tpar ≈ k/P + O(C)`, y con
`k` comparable a `C` el término `C` domina. Se arregla con un `-c` más pequeño (p. ej. `-c 4096`), a costa
de más mensajes. Su ventaja teórica (balancear carga si los procesos van a distinta velocidad) no apareció ni con
`-np 8` en una máquina con núcleos de rendimiento y de eficiencia (ver sección Extras); queda por probar en el cluster.

**Naive** — speedup 1 en b y f, ≈ 5 en e, ≈ 0.9 en a, y miles en c y d. Ninguna conclusión sobre "qué tan
bien paraleliza" es válida con una sola llave.

**Offset aleatorio** — la menos fiable: de 0.04× (llave a: empeora 25 veces) a 14× (llave f: superlineal por
suerte). Para a, el proceso 0 empezó en un offset aleatorio de su bloque de 16.7 M llaves, tuvo que recorrer
hasta el final, dar la vuelta y llegar a la llave: 3.2 M iteraciones (≈ 19 % del bloque), cuando el
secuencial la habría hallado en 123 457. Los speedups
> 4 (b, e, f) tampoco son mérito del algoritmo, solo del offset que tocó. **Aviso metodológico:** esas
corridas usaron la misma semilla por defecto (`-s 12345`), así que las 5 repeticiones tienen casi los mismos
offsets (las iteraciones varían menos de 1 %); esta tabla muestra un solo "sorteo" por llave, no su
distribución. Con 10 semillas distintas se caracteriza en la sección Extras.

### Conclusión de la comparación

| Criterio | Naive | Entrelazado | Maestro-trabajador | Offset aleatorio |
|---|---|---|---|---|
| Consistencia entre llaves | muy mala | **excelente** | **excelente** | mala |
| Peor caso | speedup 1 | ≈ P | ≈ P (llaves pequeñas: limitado por el chunk) | < 1 |
| Eficiencia (tiempo) | no aplica | **91–97 %** | 89–93 % | no aplica |
| Complejidad | baja | baja | media | baja |

El objetivo del enunciado (mejor "tiempo paralelo esperado") se cumple con el entrelazado y el
maestro-trabajador: ambos garantizan `Tpar ≈ k/P` para toda llave. A cambio renuncian a los speedups
espectaculares del naive, que son suerte y no una propiedad del algoritmo.

---

## Extras

Máquina de las pruebas: 8 núcleos, 4 de eficiencia + 4 de rendimiento (heterogéneos). Esto importa para
interpretar todo lo que sigue: los núcleos de eficiencia son más lentos, así que "8 trabajadores" no equivale
a "el doble de 4 trabajadores".

### Híbrido MPI + OpenMP frente a MPI puro (mismos 8 trabajadores)

`scripts/benchmark_extras.sh 26 8 5`: entrelazado con `-np 8` frente a híbrido `4×2`, `2×4` y `1×8`
(procesos × hilos). Tiempo promedio en segundos (5 repeticiones, CV típico 5–12 %):

| Llave | MPI 8×1 | Híbrido 4×2 | Híbrido 2×4 | Híbrido 1×8 |
|---|---|---|---|---|
| b S/4−1 | 1.448 | 1.362 | 1.639 | 1.606 |
| c S/4 | 1.364 | 1.485 | 1.738 | 1.738 |
| d S/2+1 | 2.537 | 2.604 | 3.208 | 3.443 |
| e S/2+S/8 | 3.465 | 3.688 | 3.925 | 3.938 |
| f S/7+S/13 | 1.222 | 1.336 | 1.346 | 1.554 |
| **Razón MPI/híbrido (media, b–f)** | 1.00 | **0.98** | **0.98** | **0.92** |

(La llave a se excluye: las corridas duran ~20 ms y el CV llega a 94 %, es puro ruido.)

**Lectura:** con los mismos 8 trabajadores el híbrido **no es más rápido** que MPI puro: 4×2 y 2×4 quedan
empatados dentro del ruido (razón 0.98) y 1×8 rinde ~8 % menos en promedio (hasta 36 % más tiempo en la llave d). No se puede
reportar un speedup de OpenMP sobre la versión sin OpenMP en una sola máquina, y conviene decirlo así en el
informe. Tiene sentido: la búsqueda es "vergonzosamente paralela", casi no hay comunicación que ahorrar y los
procesos MPI ya aprovechan todos los núcleos. La ventaja esperada del híbrido está en el cluster (menos
procesos MPI por nodo, menos estado duplicado, mensajes entre nodos solo desde el hilo principal), que es lo
que falta probar.

La corrida anterior del híbrido (`-np 4 -h 2`, 3.8–5.6× en tiempo) no era una comparación válida por usar el
doble de trabajadores que el entrelazado de 4 procesos; estos datos la reemplazan.

**Escalamiento de 4 a 8 trabajadores (heterogéneo).** Entrelazado, tiempos de la bitácora anterior
(`-np 4`) contra MPI 8×1 de esta: b 1.97→1.45 s, d 4.09→2.54 s, e 4.87→3.47 s. Duplicar los trabajadores
solo da ~1.4–1.6× (el speedup en tiempo pasa de ≈3.8 a ≈5.3), lo cual cuadra con que los 4 núcleos
adicionales son de eficiencia y rinden bastante menos en esta carga. (Dos sesiones distintas, tómese como
orden de magnitud.)

### Todos los acercamientos con `-np 8` (8 núcleos: 4 de rendimiento + 4 de eficiencia)

`SALIDA=resultados/bitacora_np8.csv HILOS=1 scripts/benchmark.sh 26 8 5` (5 repeticiones por combinación).
Ahora hay 8 bloques de `B = 2^23` llaves en el naive, así que **la clasificación de las llaves cambia**: el
enunciado ya advierte que hay que adaptarla al número de procesos.

Speedup en **iteraciones** (`Tsec / Tpar`):

| Llave | Naive | Entrelazado | Maestro-trabajador | Híbrido (np 8, 1 hilo) |
|---|---|---|---|---|
| a 123456 | 0.88 | 4.71 | 3.08 | 4.57 |
| b S/4−1 | **2.00** | 7.66 | 7.25 | 7.75 |
| c S/4 | 4 096 | 7.69 | 7.41 | 7.87 |
| d S/2+1 | 8 192 | 7.67 | 7.42 | 7.76 |
| e S/2+S/8 | 10 240 | 7.85 | 7.38 | 7.85 |
| f S/7+S/13 | 2.28 | 7.67 | 7.46 | 7.85 |

Speedup en **tiempo** (`Tsec / Tpar`):

| Llave | Naive | Entrelazado | Maestro-trabajador | Híbrido (np 8, 1 hilo) |
|---|---|---|---|---|
| a 123456 | 0.70 | 1.51 | 2.85 | 2.99 |
| b S/4−1 | 1.29 | 4.87 | 4.61 | 4.98 |
| c S/4 | 1 769 | 4.95 | 4.44 | 4.96 |
| d S/2+1 | 3 937 | 5.03 | 4.34 | 4.23 |
| e S/2+S/8 | 5 136 | 4.95 | 3.49* | 4.23 |
| f S/7+S/13 | 1 | 4.73 | 4.39 | 4.64 |

\* Una de las 5 corridas tardó 8.09 s (las otras, 4.29–4.86 s); con la mediana el speedup sería ≈ 4.1.
La llave a dura 10–60 ms y es ruido.

**Lectura:**

1. **El reparto del trabajo es casi perfecto; el cuello de botella es el hardware.** En iteraciones el
   entrelazado logra 7.7× con 8 trabajadores (eficiencia ≈ 96 %), pero en tiempo solo 4.7–5.0× (≈ 61 %).
   La diferencia es que los 4 núcleos de eficiencia tardan más por llave: el trabajador más cargado tarda
   ~680 ns por llave contra ~444 ns del secuencial. Para el informe conviene separar las dos medidas: la de
   iteraciones evalúa el algoritmo y la de tiempo evalúa algoritmo + máquina.
2. **El naive cambia con el número de procesos.** La llave `S/4 − 1` pasa de speedup 1.00 con 4 procesos a
   **2.00** con 8 (ahora es la última de un bloque de `S/8`, no de `S/4`). La llave `S/2 + S/8`, que con 4
   procesos quedaba a mitad de un bloque (≈ 5×), con 8 es la primera de un bloque (10 240× en iteraciones). Las
   iteraciones medidas coinciden con la fórmula `(k mod B) + 1` (p. ej. b: 8 388 608 esperadas y medidas).
3. **La hipótesis del maestro-trabajador no se cumplió.** Se esperaba que el reparto dinámico compensara los
   núcleos lentos y superara al entrelazado estático. En esta máquina ocurre lo contrario: es 8–16 % más
   lento en las llaves c, d y f (y ~19 % en e con la mediana), consistente en casi todas las repeticiones. No
   se investigó la causa; candidatos por probar: el costo de que el maestro también calcule y atienda
   solicitudes, y el tamaño de chunk (`-c`), por ejemplo `-c 4096` frente a `-c 262144`. Con llaves
   pequeñas (a) sí gana (2.85× frente a 1.51×), pero con 10–60 ms de duración eso es ruido. Su ventaja
   teórica (adaptarse a nodos de velocidades muy distintas) queda para el cluster.
4. **Híbrido con 1 hilo ≈ entrelazado**, como debe ser (es el mismo algoritmo): razón de tiempos 0.98–1.19.
   En las llaves d y e salió ~18 % más lento; puede ser ruido o costo del soporte de hilos, no se midió aparte.
5. **Offset aleatorio con semilla fija 12345:** en la llave e dio 0.14 s (127× en tiempo, 246× en
   iteraciones) en las 5 repeticiones porque el sorteo cae cerca de la llave, y 0.04× en la llave a. Es el mismo
   efecto de la tabla de 10 semillas: depende del sorteo, no de la llave.

### Offset aleatorio con 10 semillas (`-np 8`, `-s 1…10`)

Speedup en iteraciones frente al secuencial (`(k+1) / iter_max`), por llave:

| Llave | Mínimo | Mediana | Media | Máximo | Corridas con speedup < 1 |
|---|---|---|---|---|---|
| a 123456 | 0.01 | **0.02** | 0.41 | 3.77 | **9 de 10** |
| b S/4−1 | 2.01 | 5.95 | 10.85 | 34.42 | 0 de 10 |
| c S/4 | 2.00 | 4.10 | 11.18 | 73.14 | 0 de 10 |
| d S/2+1 | 4.00 | 8.54 | 13.29 | 40.11 | 0 de 10 |
| e S/2+S/8 | 6.10 | 14.78 | 30.56 | 111.30 | 0 de 10 |
| f S/7+S/13 | 1.78 | 2.55 | 182.79 | **1800.44** | 0 de 10 |

- La variabilidad es enorme: el máximo es entre 10× y 1000× el mínimo según la llave. Con semilla fija las 5
  repeticiones de la primera bitácora ocultaban esto.
- La **media** engaña (f: 182.79 por una sola corrida de 1800×); usar la **mediana** o, mejor, promediar los
  tiempos y no los speedups.
- Con llaves pequeñas es un desastre: para a, 9 de 10 corridas fueron más lentas que el secuencial (mediana
  0.02×, 50 veces más lento).
- Con llaves grandes casi siempre "gana", pero por suerte del offset, no por una propiedad del algoritmo:
  su tiempo esperado es ≈ `B/2` para cualquier llave, igual que el naive sobre llaves uniformes. Elimina el
  peor caso determinista (speedup 1) a cambio de abrir una cola larga de resultados malos y buenos.

Conclusión para el informe: el offset aleatorio **no** es una mejora del "tiempo paralelo esperado"; solo
cambia qué llaves salen perjudicadas. Es útil como contraste con el entrelazado y el maestro-trabajador,
que dan ≈ P para todas.

### Cluster MPI

Pendiente de probar con las máquinas del equipo (ver README). Para el informe conviene reportar el
speedup de `-np N` en 1 máquina frente a `-np 2N` en 2 máquinas con el maestro-trabajador y el entrelazado,
y repetir la comparación MPI puro contra híbrido ahí, que es donde se espera que OpenMP ayude.

## Pruebas con 56 bits reales (puntos 2a–2c), naive con `-np 4`

| Inciso | Llave | Resultado | Tiempo | Iteraciones (máx. por proceso) |
|---|---|---|---|---|
| a | 123456 | encontrada | 0.0608 s | 126 976 |
| b | 18014398509481983 (`2^54 − 1`) | **no encontrada** (límite 300 s) | 300.0 s | 648 437 760 |
| c | 18014398509481984 (`2^54`) | encontrada | 0.0018 s | 4 096 |

Ritmo medido en b: 648 437 760 llaves en 300 s por proceso ≈ **2.16 M llaves/s por proceso (≈ 463 ns/llave)**,
similar al secuencial de la bitácora anterior (444 ns/llave): el naive casi no pierde eficiencia por MPI.

**Estimación de lo que tardaría b (corrige la que había en la versión anterior de este documento).** La llave
`2^54 − 1` es la última del bloque del proceso 0, así que ese proceso tiene que recorrer **todo** su bloque de
`2^54` llaves él solo (speedup 1 con el naive):

    2^54 llaves / 2.16·10^6 llaves/s ≈ 8.3·10^9 s ≈ 264 años

Los otros tres procesos buscan en bloques que no contienen la llave, así que no aceleran nada. Con un reparto entrelazado la llave se hallaría tras `2^54/4` iteraciones por proceso,
≈ 66 años con 4 procesos al mismo ritmo: sigue siendo inviable, pero 4 veces menos.

**Reflexión (2d).** Las llaves b y c difieren en **una unidad** y sus tiempos en un factor de
264 años / 1.8 ms ≈ 4.5·10^12. Con `c`, el proceso 1 la prueba en su primera iteración (las 4 096 iteraciones reportadas son
las de los demás procesos hasta su primera revisión de la señal); con `b` el proceso 0 la prueba en su última. Es la ilustración extrema de
que el naive depende de la posición de la llave en el bloque, no de su valor, y de que un speedup de
"miles de millones" (c) o de 1 (b) dice poco del algoritmo: por eso se compara con el entrelazado y el
maestro-trabajador, que dan ≈ P en ambas llaves (aunque en el espacio de 56 bits ninguna de las dos
terminaría en tiempo razonable con 4 procesos).
