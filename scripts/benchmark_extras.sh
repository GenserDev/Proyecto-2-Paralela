#!/usr/bin/env bash
# Pruebas de los extras (comparaciones controladas):
#  (1) Offset aleatorio con 10 semillas distintas (con una sola semilla las 5 repeticiones del
#      benchmark.sh usan siempre los mismos offsets y no muestran su variabilidad real).
#  (2) OpenMP: los mismos N trabajadores repartidos de formas distintas entre procesos MPI e hilos.
#
# Uso: scripts/benchmark_extras.sh [BITS] [NUCLEOS] [REPETICIONES]
#   NUCLEOS = nucleos fisicos disponibles (por defecto 4). Con NUCLEOS=4 compara:
#     MPI puro -np 4  |  hibrido -np 2 -h 2  |  hibrido -np 1 -h 4   (4 trabajadores en cada caso)
#
# Salida: resultados/bitacora_extras.csv
#   clave_id,llave_real,etiqueta,procesos,hilos,bits,llave_encontrada,estado,tiempo_s,iter_max,iter_total

set -eo pipefail   # sin -u: compatible con el bash 3.2 de macOS
cd "$(dirname "$0")/.."

BITS="${1:-26}"
N="${2:-4}"
REPS="${3:-5}"
SALIDA="${SALIDA:-resultados/bitacora_extras.csv}"
PALABRA="es una prueba de"

MPIRUN=(mpirun)
if [[ -n "${HOSTFILE:-}" ]]; then MPIRUN+=(--hostfile "$HOSTFILE"); fi

if [[ ! -x ./bruteforce_hybrid || ! -x ./bruteforce_randoffset ]]; then
  echo "Compila primero con: make todo"
  exit 1
fi
mkdir -p datos resultados
printf 'Esta es una prueba de proyecto 2\n' > datos/texto.txt

S=$((1 << BITS))
IDS=(a_123456 b_S/4-1 c_S/4 d_facil_S/2+1 e_media_S/2+S/8 f_dificil_S/7+S/13)
VALORES=(123456 $((S / 4 - 1)) $((S / 4)) $((S / 2 + 1)) $((S / 2 + S / 8)) $(( (S + 6) / 7 + (S + 12) / 13 )))

echo "clave_id,llave_real,etiqueta,procesos,hilos,bits,llave_encontrada,estado,tiempo_s,iter_max,iter_total" > "$SALIDA"

registrar() { # $1=clave_id $2=llave_real $3=etiqueta ; stdin = salida del programa
  local linea
  linea=$(grep '^CSV:' | tail -n 1 | sed 's/^CSV:[^,]*,//') || true
  if [[ -n "$linea" ]]; then echo "$1,$2,$3,$linea" >> "$SALIDA"; fi
  return 0
}

for i in "${!IDS[@]}"; do
  id="${IDS[$i]}"
  llave="${VALORES[$i]}"
  archivo="datos/cifrado_${llave}.cif"
  ./des_tool cifrar "$llave" datos/texto.txt "$archivo" > /dev/null
  echo ">>> $id = $llave"

  # (1) offset aleatorio con semillas distintas
  for ((s = 1; s <= 10; s++)); do
    "${MPIRUN[@]}" -np "$N" ./bruteforce_randoffset "$archivo" "$PALABRA" "$BITS" -s "$s" \
      | registrar "$id" "$llave" "offset_semilla_$s"
  done

  # (2) mismos N trabajadores: MPI puro frente a cada combinacion procesos x hilos
  for ((r = 1; r <= REPS; r++)); do
    "${MPIRUN[@]}" -np "$N" ./bruteforce_interleaved "$archivo" "$PALABRA" "$BITS" \
      | registrar "$id" "$llave" "MPI_${N}x1"
    for ((h = 2; h <= N; h++)); do
      if (( N % h == 0 )); then
        "${MPIRUN[@]}" -np $((N / h)) ./bruteforce_hybrid "$archivo" "$PALABRA" "$BITS" -h "$h" \
          | registrar "$id" "$llave" "hibrido_$((N / h))x${h}"
      fi
    done
  done
done

echo "Guardado en $SALIDA ($(($(wc -l < "$SALIDA") - 1)) corridas)"
