# Proyecto Semestral – INFO145: Diseño y Analisis de Algoritmos

**Tecnicas de Representacion y Compresion en arreglos ordenados**  
Primer semestre 2026

---

## Descripcion

Este proyecto implementa y compara tres estrategias para representar y buscar en arreglos ordenados de gran tamaño:


Representacion Explicita (linea base) | `caso1_explicito.hpp` |
Gap-Coding + indice de muestreo (Sample) | `caso2_gapcoding.hpp` |
Partitioned Elias-Fano (PEF) | `caso3_pef.hpp` |

---

## Requisitos

- `g++` con soporte para C++17
- Sistema Linux/macOS (o WSL en Windows)
- Herramienta `make`

---

## Compilacion

```bash
make
```

Esto genera el ejecutable `./main` usando las flags `-std=c++17 -O3`

Para limpiar los archivos generados:

```bash
make clean
```

---

## Modos de ejecucion

### Modo Benchmark

Ejecuta automaticamente todos los experimentos con arreglos de distintos tamaños
(`100.000`, `1.000.000`, `10.000.000`) y distribuciones (lineal y normal con
distintas desviaciones estandar). Los resultados se guardan en `resultados_benchmark.csv`

```bash
./main --benchmark
---

**Salida:** archivo `resultados_benchmark.csv` con columnas:

---
n, distribucion, sigma,
build_c1_ns, search_c1_ns, space_c1_bytes,
build_c2_ns, search_c2_ns, space_c2_bytes,
build_c3_ns, search_c3_ns, space_c3_bytes
---

- `build_*_ns`: tiempo de construcción de la estructura en nanosegundos
- `search_*_ns`: tiempo promedio de búsqueda (promedio de 500 búsquedas) en nanosegundos
- `space_*_bytes`: espacio en memoria de la estructura en bytes
---

### Modo Archivo (`-i`)

Recibe un archivo `.csv` con numeros enteros (uno por linea o separados por coma),
construye las tres estructuras y permite buscar valores de forma interactiva

```bash
./main -i ruta/del/archivo.csv
```

**Formato del archivo CSV:**  
Enteros positivos, uno por linea o separados por coma. El programa los ordena automaticamente  
Rango de valores aceptados: `0` a `18446744073709551615` (tipo `uint64_t`, equivalente a `unsigned long long`)

**Ejemplo de archivo:**

```
2
7
10
12
16
```

**Interaccion:**

```
Valor: 10
Estructura (1/2/3): 2
  → Encontrado en posicioon 2 | Tiempo: 1240 ns
```

---

## Descripción tecnica de los algoritmos

### Caso 1 – Representación Explícita

Almacena el arreglo completo en memoria como `vector<uint64_t>`. La busqueda
usa **busqueda binaria iterativa** con complejidad O(log n) en tiempo y O(n) en espacio

Distribuciones soportadas:
- **Lineal:** `A[i] = A[i-1] + rand() % ε` con ε configurable
- **Normal (Gaussiana):** saltos generados con `std::normal_distribution<double>`

---

### Caso 2 – Gap-Coding + Sample

En lugar del arreglo original, se almacenan las **diferencias entre elementos
consecutivos** (gaps):

```
GC[0] = A[0]
GC[i] = A[i] - A[i-1]   para i > 0
```

El valor `GC[0] = A[0]` es un valor absoluto (no una diferencia) y puede ser
grande, por lo que se guarda aparte en un `uint64_t` (`primer_valor`). El resto
de los gaps se almacenan en un `vector<uint32_t>` (4 bytes por gap) en lugar de
`uint64_t` (8 bytes), ya que las diferencias entre consecutivos son valores
pequeños. **Esto reduce el arreglo de gaps a la mitad del Caso 1** y es una
decision declarada explicitamente

Para habilitar la busqueda se usa un **indice de muestreo (sample)**:
se guarda cada `b`-esimo valor original de `A` (con `b = sqrt(n)` por defecto)

**Busqueda:**
1. Busqueda binaria sobre el `sample` → acota el rango `[L, R]`.
2. Decodificacion secuencial de `GC` en ese rango acumulando gaps

---

### Caso 3 – Partitioned Elias-Fano (PEF)

Extiende Elias-Fano clasico para operar sobre gaps

**Por bloque de tamaño B (= 128):**
1. Se acumulan localmente los gaps → secuencia creciente local.
2. Se calcula `k = floor(log2(U / B))` con `U = maximo acumulado del bloque`.
3. Cada valor acumulado `v` se separa en:
   - **bits bajos** (`k` bits): los `k` bits menos significativos, escritos de
     forma contigua en un **bitmap continuo** (`vector<uint64_t>`).
   - **bits altos** (`v >> k`): se codifican en **unario** sobre un segundo
     bitmap. Como la secuencia es creciente, los cocientes altos son
     no-decrecientes; se escribe un `0` por cada incremento del cociente y un
     `1` por cada elemento. La parte alta ocupa así **~2 bits por elemento**,
     en lugar de un entero completo
4. Un **directorio de bloques** guarda, por bloque: `base_global` (valor de A
   antes del bloque), `k`, y los offsets en bits donde comienzan sus partes
   baja y alta dentro de los bitmaps globales. Esto permite saltar al bloque
   correcto durante la busqueda

**Busqueda:**
1. Busqueda binaria sobre el `sample` heredado del Caso 2 → acota `[L, R]`
2. Decodificacion PEF **navegando directamente sobre los bitmaps** en el rango
   acotado: se leen los `k` bits bajos, se reconstruye el bit alto contando el
   unario, se forma `v = (alto << k) | bajo` y se suma la `base_global`

**Nota de implementacion (declarada):** La parte alta y la parte baja se 
empaquetan **bit a bit** en bitmaps continuos de `uint64_t` usando operaciones
bitwise (`<<`, `>>`, `&`, `|`), tal como sugiere la pauta para una
implementacion estricta. No se usa el atajo de un entero por elemento, ya que
eso anularia la compresion de Elias-Fano. El analisis teorico de bits por
elemento se desarrollara en el informe

**Resultados de compresion observados** (n = 100.000, distribución lineal):
el Caso 2 ocupa ~50% del Caso 1, y el Caso 3 ~10% del Caso 1, confirmando que
cada representacion usa estrictamente menos bits por elemento que la anterior

---

## Archivos del repositorio

```
.
──> main.cpp               # Punto de entrada 
├──> caso1_explicito.hpp    # Caso 1: representación explicita + busqueda binaria
├──> caso2_gapcoding.hpp    # Caso 2: Gap-Coding + Sample
├──> caso3_pef.hpp          # Caso 3: Partitioned Elias-Fano
├──> Makefile               # Script de compilacion
└──> README.md              # Este archivo
```



