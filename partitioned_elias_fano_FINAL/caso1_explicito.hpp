#pragma once
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <chrono>
#include <iostream>
using namespace std;

/*
CASO 1: Representacion Explicita
Implementacion de control que almacena el arreglo original sin compresion. Utiliza busqueda binaria estandar con 
complejidad asintotica O(log n) en tiempo y O(n) en espacio. Ademas, provee las funciones base para la generacion 
sintetica de datos distribuciones Lineal y Normal utilizadas a lo largo del benchmark
 */

/*
Generacion Lineal: 
Construye un arreglo estrictamente creciente añadiendo un delta aleatorio uniforme en el rango [1, epsilon]
Se emplea un generador Mersenne Twister de 64 bits con semilla fija (42) para garantizar la reproducibilidad de los 
experimentos experimentales
 */
vector<uint64_t> generar_lineal(size_t n, uint64_t epsilon = 10) {
    vector<uint64_t> A(n);
    mt19937_64 rng(42);
    uniform_int_distribution<uint64_t> dist(1, epsilon);

    A[0] = dist(rng);
    for (size_t i = 1; i < n; i++) {
        A[i] = A[i - 1] + dist(rng);
    }
    return A;
}
/*
Distribucion normal: igual que la lineal pero el salto entre elementos lo sacamos de una gaussiana (media mu, 
desviacion sigma). Si por mala suerte sale un salto <= 0 lo forzamos a 1, porque si no el arreglo se desordena
y la busqueda binaria deja de funcionar
*/
vector<uint64_t> generar_normal(size_t n, double mu = 5.0, double sigma = 2.0) {
    vector<uint64_t> A(n);
    mt19937_64 rng(42);
    normal_distribution<double> dist(mu, sigma);

    A[0] = 100; //un valor inicial cualquiera
    for (size_t i = 1; i < n; i++) {
        double salto = dist(rng);
        uint64_t delta = (salto < 1.0) ? 1 : static_cast<uint64_t>(salto);
        A[i] = A[i - 1] + delta;
    }
    return A;
}

//La estructura en si. Solo es un vector de uint64 con el arreglo completo
struct Caso1 {
    vector<uint64_t> datos;

    void construir(const vector<uint64_t>& A) {
        datos = A;
    }

    //El espacio es directo:  cantidad de elementos por 8 bytes cada uno
    size_t espacio_bytes() const {
        return datos.size() * sizeof(uint64_t); 
    }
    

    /*
    Busqueda binaria clasica (iterativa). Devuelve el indice si lo encuentra o -1 si no esta. El mid: lo calculamos 
    como izq+(der-izq)/2 en vez de (izq+der)/2 para no arriesgarnos a un overflow con n gigante
    */
    int64_t buscar(uint64_t valor) const {
        int64_t izq = 0;
        int64_t der = static_cast<int64_t>(datos.size()) - 1;

        while (izq <= der) {
            int64_t mid = izq + (der - izq) / 2;

            if (datos[mid] == valor) {
                return mid;
            } else if (datos[mid] < valor) {
                izq = mid + 1;
            } else {
                der = mid - 1;
            }
        }
        return -1;
    }
};

/*
 Medicion de tiempo. Esta parte nos costo: al principio nos daba 0 ns porque el compilador con -O3 se daba cuenta 
 que no usabamos el resultado y borraba el ciclo entero. La solucion fue dos cosas:
   1) generar todas las consultas ANTES de arrancar el cronometro, asi no medimos el tiempo del generador random
   2) encadenar las busquedas (cada resultado le suma algo a la siguiente consulta con acc&1), para que el compilador 
   no pueda adivinar nada y se vea obligado a ejecutar todo de verdad
Al final devolvemos el promedio por busqueda
 */
double medir_tiempo_busqueda_caso1(const Caso1& c1, int repeticiones) {
    size_t n = c1.datos.size();

    mt19937_64 rng(99);
    uniform_int_distribution<size_t> dist(0, n - 1);
    vector<uint64_t> consultas(repeticiones);
    for (int i = 0; i < repeticiones; i++) consultas[i] = c1.datos[dist(rng)];

    auto inicio = chrono::high_resolution_clock::now();
    int64_t acc = 0;
    for (int i = 0; i < repeticiones; i++) {
        uint64_t v = consultas[i] + (acc & 1);
        int64_t r = c1.buscar(v);
        acc += (r >= 0) ? r : 1;
    }
    auto fin = chrono::high_resolution_clock::now();
    volatile int64_t sumidero = acc; (void)sumidero; //para que no borre el ciclo

    double total_ns = chrono::duration_cast<chrono::nanoseconds>(fin - inicio).count();
    return total_ns / repeticiones;
}