#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <random>
#include <iostream>
#include <algorithm>
using namespace std;

/*
Caso 2: Representacion con Gap-Coding
 
En vez de guardar los numeros completos, guardamos solo la diferencia entre uno y el siguiente de los gaps. Como vienen 
ordenados, las diferencias son numeros chicos. Para leer un valor tendriamos que sumar desde el inicio, asi que usamos 
un "sample": cada cierto salto guardamos el numero real para acortar el tramo que tenemos que sumar
 
Decisiones clave para ahorrar espacio:
- Metimos los gaps en uint32_t( 4 bytes) en vez de uint64_t (8 bytes). Como son diferencias pequeñas, nos sobra con 32 bits
Solo con esto, el arreglo ya pesa la mitad que el del Caso 1
-El primer valor (GC[0]) no es una diferencia, es el numero real A[0] y puede ser gigante. Ese lo guardamos aparte en un 
uint64_t para que no se nos desborde nada
 */

struct Caso2 {
    uint64_t primer_valor; // A[0] guardado aparte (puede ser grande)
    vector<uint32_t> GC; // los gaps, de la posicion 1 en adelante
    vector<uint64_t> sample;//valores reales cada "salto" posiciones
    size_t salto;// cada cuanto guardamos una muestra (la b)
    size_t n;

    // Construye los gaps y el sample a partir del arreglo original
    void construir(const vector<uint64_t>& A, size_t b= 0) {
        n =A.size() ;

        // Si no nos pasan un salto, usamos sqrt(n) que es un valor razonable (balancea el tamaño del sample con lo que hay 
        //que decodificar)
        salto = (b == 0)? max((size_t)1, (size_t)sqrt((double)n) ) : b ;

        //Calcular los gaps. GC[0] no se usa (el A[0] real esta en primer_valor), y de ahi en adelante cada GC[i] es A[i] - A[i-1]
        primer_valor = A[0];
        GC.resize(n) ;
        GC[0] =0;
        for (size_t i = 1; i < n; i++ ){
            GC[i] = static_cast<uint32_t>(A[i]- A[i - 1]);
        }

        //armar el sample : guardamos A[0], A[salto], A[2*salto], etc, osea los valores reales en posiciones espaciadas
        size_t m= (n+ salto - 1)/ salto;
        sample.reserve(m);
        for (size_t i = 0; i < n; i+= salto ) {
            sample.push_back(A[ i]) ;
        }
    }

    /*
    Busqueda; Son dos pasos:
     1)Busqueda binaria sobre el sample para ubicar en que "tramo" cae el valor (entre que dos muestras esta). Eso nos da 
     un rango [L,R] chico
     2) Dentro de ese tramo vamos sumando gaps uno por uno hasta llegar al valor o pasarnos. Como esta ordenado, si nos 
     pasamos es que no esta
     */
    int64_t buscar( uint64_t valor) const {
        // Paso 1: binaria en el sample. Buscamos el mayor sample[bloque] <=valor
        int64_t izq = 0;
        int64_t der= static_cast<int64_t>(sample.size() ) - 1;
        int64_t bloque = 0 ;

        while (izq <= der) {
            int64_t mid = izq+ (der -izq)/2;
            if ( sample[mid] == valor) {
                return mid * static_cast<int64_t>(salto); 
            } else if (sample[mid] < valor){
                bloque= mid;
                izq = mid +1;
            } else {
                der = mid -1;
            }
        }

        // Si el valor es mas chico que la primera muestra, no puede estar
        if (valor <sample[0]) return -1;

        // Paso 2: decodificar el tramo sumando gaps desde la muestra
        size_t pos_inicio = static_cast<size_t>(bloque) * salto ; 
        size_t pos_fin= min(pos_inicio + salto, n) - 1;

        uint64_t acum = sample[bloque];
        if (acum== valor) return static_cast<int64_t>(pos_inicio);

        for (size_t i = pos_inicio + 1; i <= pos_fin; i++) {
            acum +=GC[i];
            if (acum ==valor) return static_cast<int64_t>(i);
            if (acum > valor)  return -1; //nos pasamos,no esta
        }
        return -1;
    }

    //espacio : los gaps (uint32)+ el sample (uint64)+ el primer valor
    size_t espacio_bytes() const {
        return GC.size() *sizeof(uint32_t) + sample.size() * sizeof(uint64_t)+ sizeof(uint64_t);
    }
};

//misma medicion de tiempo que el caso1 
double medir_tiempo_busqueda_caso2(const Caso2& c2,const vector<uint64_t>& A_ref,int repeticiones) {
    size_t n = A_ref.size();
    mt19937_64 rng(99);
    uniform_int_distribution<size_t> dist(0, n - 1) ;
    vector<uint64_t> consultas(repeticiones);
    for (int i = 0; i < repeticiones; i++) consultas[i] = A_ref[dist(rng)];

    auto inicio = chrono::high_resolution_clock::now();
    int64_t acc= 0;
    for (int i = 0; i< repeticiones; i++) {
        uint64_t v = consultas[i] + (acc & 1);
        int64_t r= c2.buscar(v);
        acc += (r >= 0 ) ? r : 1 ;
    }
    auto fin =chrono::high_resolution_clock::now( );
    volatile int64_t resultado_temporal =acc; (void)resultado_temporal;

    double total_ns =chrono::duration_cast<chrono::nanoseconds> (fin - inicio).count() ;
    return total_ns/repeticiones ;
}
