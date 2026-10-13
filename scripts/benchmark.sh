#!/usr/bin/env bash
# Bitacora de pruebas (Parte B, puntos 2 y 5): corre secuencial y todas las variantes paralelas
# con las llaves facil / media / dificil del enunciado, escaladas al espacio 2^BITS.
#
# Uso:  scripts/benchmark.sh [BITS] [PROCESOS] [REPETICIONES]
#   BITS          espacio de busqueda 2^BITS (por defecto 24; ~16M llaves)
#   PROCESOS      procesos MPI (por defecto 4, como pide el enunciado)
#   REPETICIONES  corridas por combinacion (por defecto 3)
# Variables opcionales: HILOS (hilos OpenMP para la version hibrida, por defecto 2),
#                       HOSTFILE (para correr en un cluster), SALIDA (archivo CSV).
#
# Salida: resultados/bitacora.csv con una fila por corrida:
#   clave_id,llave_real,programa,procesos,hilos,bits,llave_encontrada,estado,tiempo_s,iter_max,iter_total

set -eo pipefail   # sin -u: compatible con el bash 3.2 de macOS
cd "$(dirname "$0")/.."

BITS="${1:-24}"
NP="${2:-4}"
REPS="${3:-3}"
HILOS="${HILOS:-2}"
SALIDA="${SALIDA:-resultados/bitacora.csv}"
TEXTO="datos/texto.txt"
PALABRA="es una prueba de"

MPIRUN=(mpirun)
[[ -n "${HOSTFILE:-}" ]] && MPIRUN+=(--hostfile "$HOSTFILE")

[[ -x ./secuencial && -x ./des_tool ]] || { echo "Compila primero con 'make' (y 'make hybrid')"; exit 1; }
mkdir -p datos resultados
printf 'Esta es una prueba de proyecto 2\n' > "$TEXTO"

# Llaves del enunciado, escaladas a S = 2^BITS (con 56 bits son exactamente las del enunciado)
S=$((1 << BITS))
# (bash 3.2 de macOS no tiene arreglos asociativos: se usan dos arreglos paralelos)
IDS=(a_123456 b_S/4-1 c_S/4 d_facil_S/2+1 e_media_S/2+S/8 f_dificil_S/7+S/13)
VALORES=(
  123456
  $((S / 4 - 1))
  $((S / 4))
  $((S / 2 + 1))
  $((S / 2 + S / 8))
  $(( (S + 6) / 7 + (S + 12) / 13 ))   # techo de S/7 + techo de S/13
)

PARALELOS=(bruteforce_mpi bruteforce_interleaved bruteforce_masterworker bruteforce_randoffset)
[[ -x ./bruteforce_hybrid ]] && PARALELOS+=(bruteforce_hybrid)

echo "clave_id,llave_real,programa,procesos,hilos,bits,llave_encontrada,estado,tiempo_s,iter_max,iter_total" > "$SALIDA"

registrar() { # $1=clave_id $2=llave_real ; stdin = salida del programa
  local linea
  linea=$(grep '^CSV:' | tail -n 1 | sed 's/^CSV://') || true
  if [[ -n "$linea" ]]; then echo "$1,$2,$linea" >> "$SALIDA"; fi
  return 0
}

for i in "${!IDS[@]}"; do
  id="${IDS[$i]}"
  llave="${VALORES[$i]}"
  archivo="datos/cifrado_${llave}.cif"
  ./des_tool cifrar "$llave" "$TEXTO" "$archivo" > /dev/null
  echo ">>> Llave $id = $llave"
  for ((r = 1; r <= REPS; r++)); do
    ./secuencial "$archivo" "$PALABRA" "$BITS" | registrar "$id" "$llave"
    for prog in "${PARALELOS[@]}"; do
      if [[ "$prog" == bruteforce_hybrid ]]; then
        "${MPIRUN[@]}" -np "$NP" "./$prog" "$archivo" "$PALABRA" "$BITS" -h "$HILOS" | registrar "$id" "$llave"
      else
        "${MPIRUN[@]}" -np "$NP" "./$prog" "$archivo" "$PALABRA" "$BITS" | registrar "$id" "$llave"
      fi
    done
  done
done

echo "Bitacora guardada en $SALIDA ($(($(wc -l < "$SALIDA") - 1)) corridas)"
python3 scripts/resumen.py "$SALIDA"
