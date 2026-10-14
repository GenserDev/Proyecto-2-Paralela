#!/usr/bin/env python3
"""Genera la grafica de speedups por llave y acercamiento a partir de resultados/bitacora.csv.

Uso: scripts/grafica.py [bitacora.csv] [salida.png]
Requiere matplotlib (pip install matplotlib).
"""
import csv
import sys
from collections import defaultdict
from statistics import mean

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ruta = sys.argv[1] if len(sys.argv) > 1 else "resultados/bitacora.csv"
salida = sys.argv[2] if len(sys.argv) > 2 else "resultados/speedups.png"

with open(ruta, newline="") as f:
    filas = list(csv.DictReader(f))

datos = defaultdict(list)
claves = []
for fila in filas:
    datos[(fila["clave_id"], fila["programa"])].append(fila)
    if fila["clave_id"] not in claves:
        claves.append(fila["clave_id"])

NOMBRES = {
    "naive": "Naive",
    "entrelazado": "Entrelazado",
    "maestro_trabajador": "Maestro-trabajador",
    "offset_aleatorio": "Offset aleatorio",
    "hibrido_entrelazado": "Híbrido MPI+OpenMP",
}
programas = [p for p in NOMBRES if any((c, p) in datos for c in claves)]


def speedup(clave, programa, campo):
    sec = mean(float(r[campo]) for r in datos[(clave, "secuencial")])
    par = mean(float(r[campo]) for r in datos[(clave, programa)])
    return sec / par


bits = filas[0]["bits"]
procesos = next(f["procesos"] for f in filas if f["programa"] != "secuencial")
fig, ejes = plt.subplots(1, 2, figsize=(14, 5))
ancho = 0.8 / len(programas)
for eje, (campo, titulo) in zip(
    ejes,
    [("iter_max", "Speedup en iteraciones (independiente del hardware)"),
     ("tiempo_s", "Speedup en tiempo (T_sec / T_par)")],
):
    for i, programa in enumerate(programas):
        desplazamiento = (i - (len(programas) - 1) / 2) * ancho
        eje.bar([x + desplazamiento for x in range(len(claves))],
                [speedup(c, programa, campo) for c in claves], ancho, label=NOMBRES[programa])
    eje.set_yscale("log")
    eje.set_xticks(range(len(claves)))
    eje.set_xticklabels([c.replace("_", "\n", 1) for c in claves], fontsize=8)
    eje.axhline(1, color="gray", lw=0.8, ls="--")
    eje.axhline(int(procesos), color="black", lw=0.8, ls=":")
    eje.set_title(titulo, fontsize=10)
    eje.set_ylabel("speedup (escala log)")
    eje.grid(axis="y", alpha=0.3)
ejes[0].legend(fontsize=8, loc="upper left")
fig.suptitle(f"Speedup por llave y acercamiento (espacio 2^{bits}, -np {procesos}; "
             f"punteada = {procesos} = ideal con {procesos} procesos)", fontsize=11)
fig.tight_layout()
fig.savefig(salida, dpi=150)
print("Grafica guardada en", salida)
