#!/usr/bin/env python3
"""Resume resultados/bitacora.csv: tiempos promedio y speedups por llave y por algoritmo.

Speedup en tiempo     = T_secuencial / T_paralelo           (depende del hardware)
Speedup en iteraciones = iter_secuencial / iter_max_paralelo (no depende del hardware: mide
                          cuantas llaves prueba el trabajador mas cargado, es decir el
                          "tiempo paralelo esperado" en iteraciones que usa el enunciado)
"""
import csv
import sys
from collections import defaultdict
from statistics import mean

ruta = sys.argv[1] if len(sys.argv) > 1 else "resultados/bitacora.csv"
with open(ruta, newline="") as f:
    filas = list(csv.DictReader(f))
if not filas:
    sys.exit("La bitacora esta vacia")

claves, programas = [], []
datos = defaultdict(list)
for fila in filas:
    if fila["clave_id"] not in claves:
        claves.append(fila["clave_id"])
    if fila["programa"] not in programas:
        programas.append(fila["programa"])
    datos[(fila["clave_id"], fila["programa"])].append(fila)

def promedio(clave, programa, campo):
    corridas = [r for r in datos.get((clave, programa), []) if r["estado"] == "encontrada"]
    return mean(float(r[campo]) for r in corridas) if corridas else None

def formato(valor, decimales=2):
    return "-" if valor is None else f"{valor:.{decimales}f}"

def tabla(titulo, funcion):
    print(f"\n### {titulo}\n")
    print("| Llave | " + " | ".join(programas) + " |")
    print("|---|" + "---|" * len(programas))
    for clave in claves:
        print(f"| {clave} | " + " | ".join(funcion(clave, p) for p in programas) + " |")

bits = filas[0]["bits"]
procesos = next((f["procesos"] for f in filas if f["programa"] != "secuencial"), "?")
print(f"\n## Resumen (espacio 2^{bits}, {len(filas)} corridas; los paralelos usan -np {procesos})")

tabla("Tiempo promedio (s)", lambda c, p: formato(promedio(c, p, "tiempo_s"), 4))
tabla("Iteraciones del trabajador mas cargado", lambda c, p: formato(promedio(c, p, "iter_max"), 0))

def speedup_tiempo(c, p):
    sec, par = promedio(c, "secuencial", "tiempo_s"), promedio(c, p, "tiempo_s")
    return "-" if not sec or not par else formato(sec / par)

def speedup_iter(c, p):
    sec, par = promedio(c, "secuencial", "iter_max"), promedio(c, p, "iter_max")
    return "-" if not sec or not par else formato(sec / par)

tabla("Speedup en tiempo (T_sec / T_par)", speedup_tiempo)
tabla("Speedup en iteraciones (independiente del hardware)", speedup_iter)
