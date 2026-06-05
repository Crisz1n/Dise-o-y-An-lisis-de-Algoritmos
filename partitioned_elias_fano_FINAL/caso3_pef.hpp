#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <random>
#include <iostream>
#include <cassert>
#include <algorithm>
using namespace std;

/*
Caso 3- Partitioned Elias-Fano (PEF). Este es nuestro algoritmo asignado

El problema: Elias-Fano normal necesita que la secuencia sea creciente, pero los gaps no lo son. La movida de PEF 
es partir el arreglo en bloques y, dentro de cada bloque, sumar los gaps acumulando para que ese pedazo si quede creciente. Sobre 
esa secuencia creciente ya podemos aplicar Elias-Fano

Como funciona EF en cada bloque:
 - Parte BAJA: los k bits de mas abajo de cada numero se guardan tal cual, pegados uno al lado del otro
 - Parte ALTA: el resto (v >> k) NO se guarda como numero, se guarda en UNARIO. Como la secuencia crece
nunca, asi que ponemos un '0' cada vez que sube y un '1' por cada elemento. Eso termina ocupando como 2 bits por elemento nomas, 
sin importar lo grande que sea el valor

Al principio nos habiamos equivocado y guardabamos la parte alta como un uint64 por elemento, y obvio que no comprimia nada (ocupaba 
MAS que el Caso
1). Cuando lo pasamos a unario de verdad recien empezo a comprimir bien

Todo se empaqueta bit a bit en un bitmap continuo de uint64_t con operaciones
<<, >>, &, |, que es lo que pide la pauta para el Caso 3
 */

/*
Bitmap: nuestra "tira de bits" continua. Por dentro es un vector de uint64, y cada uint64 nos da 64 bits para usar
Tiene metodos para escribir/leer un bit suelto o varios de una
 */
struct Bitmap {
    vector<uint64_t> palabras ;   //cada palabra = 64 bits
    size_t n_bits= 0;              // cuantos bits llevamos escritos

    void reservar_bits( size_t bits) {
        palabras.reserve( (bits + 63) / 64);
    }

    //Escribir un bit al final. Calculamos en que palabra cae (n_bits/64) y en que posicion dentro de esa palabra 
    //(n_bits%64) y prendemos el bit si toca
    void writeBit(int bit) {
        size_t palabra = n_bits >> 6;           // /64
        size_t offset  = n_bits & 63;           // %64
        if (palabra>= palabras.size()) palabras.push_back(0ULL);
        if (bit) palabras[palabra] |= (1ULL << offset);
        n_bits++;
    }

    // Escribir varios bits (los 'ancho' bits de mas abajo de 'valor'), del menos significativo al mas significativo
    void writeBits(uint64_t valor,int ancho) {
        for (int i = 0; i <ancho; i++ ) {
            writeBit(static_cast<int>((valor >> i) & 1ULL) );
        }
    }

    //leer un bit en una posicion dada
    int readBit(size_t pos) const {
        size_t palabra = pos>> 6;
        size_t offset= pos & 63;
        return static_cast<int>((palabras[palabra] >>  offset) &1ULL);
    }

    // Leer 'ancho' bits a partir de 'pos' y armar el numero de vuelta
    uint64_t readBits(size_t pos, int ancho) const {
        uint64_t valor= 0 ;
        for (int i = 0; i< ancho; i++) {
            if (readBit(pos + i)) valor |= (1ULL << i);
        }
        return valor;
    }


    size_t espacio_bytes() const {
        return palabras.size()* sizeof(uint64_t) ; 
    }
} ;

/*
Lo que se guarda por cada bloque. Con esto podemos saltar directo al bloque sin recorrer todo
sabemos donde empieza su parte baja y su parte alta en los bitmaps, cuanto vale A justo antes del bloque (base_global) y que k uso
 */
struct BloquePEF {
    uint64_t base_global;
    uint32_t cant;
    uint8_t  k;
    size_t   off_bajos;
    size_t   off_altos;
};

struct Caso3 {
    Bitmap bajos ; // todos
    Bitmap altos;    //toda la parte alta en unario
    vector<BloquePEF> bloques ; // el directorio
    vector<uint64_t>  sample;  // el mismo sample del caso2
    size_t salto ;
    size_t n;
    size_t tam_bloque; // B (lo dejamos en 128 )

    /*
    Construccion: Recorremos el arreglo de gaps de a bloques de B. Por cada bloque: acumulamos los gaps para que quede creciente, 
    elegimos un k, y escribimos los bits bajos y los altos en los bitmaps. De paso vamos anotando en el directorio
     */
    void construir(const vector<uint32_t>& GC,
                   uint64_t primer_valor,
                   const vector<uint64_t>& sample_c2 ,
                   size_t salto_c2,
                   size_t B= 128)
    {
        n = GC.size();
        salto =salto_c2;
        sample= sample_c2;
        tam_bloque =B;

        size_t num_bloques= (n + B-1) /B ;
        bloques.resize(num_bloques) ;

        //va llevando cuanto vale A hasta el inicio del bloque actual
        uint64_t acum_global = 0;

        for (size_t b = 0; b< num_bloques; b++) {
            size_t inicio = b * B;
            size_t fin= min(inicio + B,n);
            size_t cant= fin -inicio;

            BloquePEF& blq= bloques[b];
            blq.cant=static_cast<uint32_t>(cant);
            blq.off_bajos=bajos.n_bits; // donde arranca este bloque en los bitmaps
            blq.off_altos = altos.n_bits;

            //acumular los gaps del bloque (El A[0] real no esta en GC[0], se mete despues via base_global)
            vector<uint64_t> acum_local(cant);
            uint64_t s = 0;
            for (size_t j = 0; j < cant; j++) {
                s += GC[inicio+ j];
                acum_local[j] = s;
            }

            // La base del bloque: cuanto vale A justo antes de empezar el bloque
            // En el primer bloque es el primer_valor (A[0]); en los demas es lo acumulado de los bloques anteriores
            if (b ==0) {
                blq.base_global =primer_valor;
            } else {
                blq.base_global = acum_global;
            }

            //Elegir el k del bloque:la formula de la pauta es floor(log2(U/B)) , con U="el maximo acumulado". Lo capamos 
            // a [0,58] por las dudas para que no se desborde nada al hacer los desplazamientos
            uint64_t U= (cant > 0) ? acum_local[cant - 1] :0;
            int k = 0;
            if (U > 0 && cant > 0) {
                double ratio = static_cast<double>(U) / static_cast<double>(cant);
                k = (ratio >= 1.0) ? static_cast<int>(floor(log2(ratio))) : 0;
            }
         
            if (k < 0) k = 0;
            if (k > 58) k= 58;
            blq.k = static_cast<uint8_t>(k);

            //Escribir la parte baja: los k bits de abajo de cada valor, seguidos
            uint64_t mascara_bajos =(k > 0) ? ( (1ULL << k) -1) : 0ULL ;
            for (size_t j = 0; j < cant; j++ ) {
                uint64_t low = acum_local[j] & mascara_bajos ;
                bajos.writeBits( low, k); // si k es 0 simplemente no escribe nada
            }

            //Escribir la parte alta en unario: por cada elemento ponemos tantos '0' como haya subido el cociente respecto 
            // al anterior, y cerramos con un '1' que marca "aca termina este elemento"
            uint64_t h_prev =0;
            for (size_t j = 0; j< cant; j++ ) {
                uint64_t h = (k> 0) ? (acum_local[j] >> k) :acum_local[j] ;
                uint64_t ceros = h - h_prev;
                for (uint64_t z = 0; z < ceros; z++) altos.writeBit(0);
                altos.writeBit(1);
                h_prev= h;
            }

            //Dejar listo el acumulado para el proximo bloque
            acum_global= blq.base_global + ((cant > 0) ? acum_local[cant - 1] : 0);
        }
    }

    /*
    Decodificar el valor A[i] leyendo directo de los bitmaps(sin tener el arreglo original). Pasos:ubicamos bloque y 
    posicion local, leemos los k bits bajos, y reconstruimos la parte alta recorriendo el unario (contando unos hasta 
    llegar a nuestro elemento, y los ceros del camino son el cociente). Despues juntamos las dos partes y le sumamos la base
     */
    uint64_t decodificar(size_t i) const{
     
        size_t b= i /tam_bloque;
        size_t j = i %tam_bloque;
        const BloquePEF& blq= bloques[b];
        int k = blq.k ;

        // parte baja de la posicion j
        uint64_t low= (k >0) ? bajos.readBits(blq.off_bajos +j * k, k) :0ULL;

        // parte alta: avanzamos por el unario contando unos (elementos) hasta el j que buscamos; cada cero que pasamos 
        //suma 1 al cociente
        size_t pos =blq.off_altos ;
        uint64_t h= 0;
        size_t unos_vistos = 0;
        while (true ) {
            int bit= altos.readBit(pos++);
            if (bit == 1) {
                if (unos_vistos ==j) break; //este '1' es el de nuestro elemento
                unos_vistos++;
            } else {
                h++ ;
            }
        }
     
     
        uint64_t valor_local = (static_cast<uint64_t>( h) << k) | low;
        return blq.base_global + valor_local;
    }

    /*
    Busqueda. Igual que el caso2 en el paso 1 (binaria sobre el sample para achicar el rango), pero en el paso 2 
    en vez de sumar gaps vamos decodificando posicion por posicion sobre los bitmaps comprimidos
     */
    int64_t buscar(uint64_t valor) const {
        int64_t izq = 0;
        int64_t der = static_cast<int64_t>(sample.size()) - 1;
        int64_t bloque_sample = 0;
     

        while (izq <= der) {
            int64_t mid = izq + (der - izq) / 2;
            if (sample[mid] == valor) {
                return mid * static_cast<int64_t>(salto); 
            } else if (sample[mid] < valor) {
                bloque_sample = mid;
                izq = mid + 1;
            } else {
                der = mid - 1;
            }
        }

        if (valor < sample[0]) return -1;

        // recorrer el tramo decodificando hasta encontrarlo o pasarnos
        size_t pos_inicio = static_cast<size_t>(bloque_sample) * salto; 
        size_t pos_fin    = min(pos_inicio + salto, n) - 1;

        for (size_t i = pos_inicio; i <= pos_fin; i++) {
            uint64_t v = decodificar(i);
            if (v == valor) return static_cast<int64_t>(i);
            if (v >  valor) return -1;
        }
        return -1;
    }

    // Espacio: los dos bitmaps+ el directorio de bloques+ el sample
    size_t espacio_bytes() const {
        size_t total = 0;
        total += bajos.espacio_bytes();
        total += altos.espacio_bytes();
        total += bloques.size() * sizeof(BloquePEF); 
        total += sample.size() * sizeof(uint64_t);   
        return total;
    }
};


// Misma medicion de siempre (consultas pre-generadas y encadenadas para que el compilador no nos borre el ciclo)
double medir_tiempo_busqueda_caso3( const Caso3& c3,
                                   const vector<uint64_t>& A_ref,
                                   int repeticiones) {
    size_t n = A_ref.size();
    mt19937_64 rng(99) ;
    uniform_int_distribution<size_t> dist(0, n-1) ;
    vector<uint64_t> consultas(repeticiones);
    for (int i = 0; i < repeticiones; i++ ) consultas[i]= A_ref[dist(rng)];

    auto inicio = chrono::high_resolution_clock::now( );
    int64_t acc = 0;
    for (int i = 0; i < repeticiones; i++){
        uint64_t v = consultas[i]+ (acc & 1);
        int64_t r = c3.buscar(v) ;
        acc += (r >= 0) ? r : 1 ;
    }
 
    auto fin= chrono::high_resolution_clock::now( );
    volatile int64_t sumidero= acc; (void)sumidero;

    double total_ns =chrono::duration_cast<chrono::nanoseconds>(fin - inicio).count();
    return total_ns/repeticiones ;
}
