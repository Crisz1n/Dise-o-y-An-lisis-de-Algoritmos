#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <algorithm>

#include "caso1_explicito.hpp"
#include "caso2_gapcoding.hpp"
#include "caso3_pef.hpp"
using namespace std;

/*
main del proyecto 2 modos
  modo 1:./main --benchmark         -> corre todas las pruebas solo y deja un CSV
  modo 2: ./main -i archivo.csv      -> carga un csv y deja buscar a mano
 */

void mostrar_uso(const char* nombre_programa) { 
    cout << "Uso:\n";
    cout << "  " << nombre_programa << " --benchmark\n";
    cout << "  " << nombre_programa << " -i ruta/del/archivo.csv\n";
}

/*
Arma los tres casos para un arreglo dado, mide todo construccion, busqueda, espacio y devuelve una linea lista para el CSV
el Caso 3 es mucho mas lento que los otros dos porque decodifica bit a bit. Por eso a C3 le damos bastante
 */
string ejecutar_experimento(const vector<uint64_t>& A,
                                 size_t n,
                                 const string& dist_nombre,
                                 double sigma,
                                 int repeticiones_busqueda = 1000)
{
    int rep_rapidos = repeticiones_busqueda;                      //C1 y C2
    int rep_lento   = max(500, repeticiones_busqueda / 100); //C3

    //Caso 1
    auto t0 = chrono::high_resolution_clock::now();
    Caso1 c1;
    c1.construir(A);
    auto t1 = chrono::high_resolution_clock::now();
    double build_c1 = chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();
    double search_c1 = medir_tiempo_busqueda_caso1(c1, rep_rapidos);
    size_t space_c1  = c1.espacio_bytes();

    // Caso2
    t0 = chrono::high_resolution_clock::now();
    Caso2 c2;
    c2.construir(A); // el salto lo decide solo (sqrt(n))
    t1 = chrono::high_resolution_clock::now();
    double build_c2 = chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();
    double search_c2 = medir_tiempo_busqueda_caso2(c2, A, rep_rapidos);
    size_t space_c2  = c2.espacio_bytes();

    //Caso 3 (PEF), reusa los gaps y el sample del Caso 2 con B = 128 por bloque
    t0 = chrono::high_resolution_clock::now();
    Caso3 c3;
    c3.construir(c2.GC, c2.primer_valor, c2.sample, c2.salto, 128);
    t1 = chrono::high_resolution_clock::now();
    double build_c3 = chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();
    double search_c3 = medir_tiempo_busqueda_caso3(c3, A, rep_lento);
    size_t space_c3  = c3.espacio_bytes();

    //Armar la fila del CSV con todo junto
    ostringstream oss;
    oss << n         << ","
        << dist_nombre << ","
        << sigma     << ","
        << (long long)build_c1  << "," << search_c1  << "," << space_c1  << ","
        << (long long)build_c2  << "," << search_c2  << "," << space_c2  << ","
        << (long long)build_c3  << "," << search_c3  << "," << space_c3;

    return oss.str();
}

/*
Modo benchmark: genera datos para varios tamaños y distribuciones, mide todo y va escribiendo cada fila en el CSV
Este es el modo que sirve para sacar los numeros del informe (los graficos del Hito 2 salen de aca)
*/
void modo_benchmark() {
    cout << "[BENCHMARK] Iniciando experimentos...\n\n";

    // Tamaños en potencias de 10, como sugiere la pauta
    vector<size_t> tamanos = {100000, 1000000, 10000000};

    //Varios sigmas para la normal, para ver como afecta la dispersion
    vector<double> sigmas = {2.0, 5.0, 10.0};

    uint64_t epsilon = 10;     // para la lineal
    int repeticiones = 100000; //busquedas por experimento (se promedian)

    ofstream csv("resultados_benchmark.csv");
    if (!csv.is_open()) {
        cerr << "Error: no se pudo crear resultados_benchmark.csv\n";
        return;
    }

    // Encabezado del CSV
    csv << "n,distribucion,sigma,"
        << "build_c1_ns,search_c1_ns,space_c1_bytes,"
        << "build_c2_ns,search_c2_ns,space_c2_bytes,"
        << "build_c3_ns,search_c3_ns,space_c3_bytes\n";

    for (size_t n : tamanos) {
        //Primero la lineal
        cout << "n=" << n << " | Lineal (epsilon=" << epsilon << ")... ";
        cout.flush();
        vector<uint64_t> A_lin = generar_lineal(n, epsilon);
        string fila_lin = ejecutar_experimento(A_lin, n, "lineal", 0.0, repeticiones);
        csv << fila_lin << "\n";
        csv.flush();
        cout << "OK\n";

        //Despues la normal con cada sigma
        for (double sigma : sigmas) {
            cout << "n=" << n << " | Normal (sigma=" << sigma << ")... ";
            cout.flush();
            vector<uint64_t> A_norm = generar_normal(n, 5.0, sigma);
            string fila_norm = ejecutar_experimento(A_norm, n, "normal", sigma, repeticiones);
            csv << fila_norm << "\n";
            csv.flush();
            cout << "OK\n";
        }
    }

    csv.close();
    cout << "\n[BENCHMARK] Resultados guardados en resultados_benchmark.csv\n";
}

/*
Lee un CSV de enteros. Acepta un numero por linea o varios separados por coma. Si algun token no es  numero lo ignora.
 */
vector<uint64_t> leer_csv(const string& ruta) {
    vector<uint64_t> valores;
    ifstream archivo(ruta);
    if (!archivo.is_open()) {
        cerr << "Error: no se pudo abrir el archivo " << ruta << "\n";
        return valores;
    }

    string linea;
    while (getline(archivo, linea)) {
        istringstream ss(linea);
        string token;
        while (getline(ss, token, ',')) {
            token.erase(remove_if(token.begin(), token.end(), ::isspace), token.end());
            if (!token.empty()) {
                try {
                    valores.push_back(static_cast<uint64_t>(stoull(token)));
                } catch (...) {
                    //si no era un numero lo saltamos
                }
            }
        }
    }
    return valores;
}

/*
Modo archivo: carga el csv, lo ordena, arma las tres estructuras y deja al usuario buscar 
valores a mano eligiendo en cual de las tres buscar. Muestra si lo encontro y en que posicion

 */
void modo_archivo(const string& ruta) {
    cout << "[ARCHIVO] Leyendo: " << ruta << "\n";
    vector<uint64_t> A = leer_csv(ruta);

    if (A.empty()) {
        cerr << "Error: archivo vacío o sin datos validos.\n";
        return;
    }

    sort(A.begin(), A.end()); // por si venia desordenado
    size_t n = A.size();
    cout << "[ARCHIVO] " << n << " elementos cargados.\n\n";

    cout << "Construyendo estructuras...\n";

    Caso1 c1; c1.construir(A);
    Caso2 c2; c2.construir(A);
    Caso3 c3; c3.construir(c2.GC, c2.primer_valor, c2.sample, c2.salto, 128);

    cout << "Listo. Espacio usado:\n";
    cout << "  Caso 1 (Explícito):          " << c1.espacio_bytes() << " bytes\n";
    cout << "  Caso 2 (Gap-Coding + Sample): " << c2.espacio_bytes() << " bytes\n";
    cout << "  Caso 3 (PEF + Sample):        " << c3.espacio_bytes() << " bytes\n\n";

    cout << "Ingrese un valor a buscar (o 'q' para salir).\n";
    cout << "Luego elija la estructura: 1=Explicito, 2=GapCoding, 3=PEF\n\n";

    string entrada;
    while (true) {
        cout << "Valor: ";
        getline(cin, entrada);
        if (entrada == "q" || entrada == "Q") break;

        uint64_t valor = 0;
        try {
            valor = static_cast<uint64_t>(stoull(entrada));
        } catch (...) {
            cout << "Entrada invalida. Ingrese un entero o 'q' para salir.\n";
            continue;
        }

        cout << "Estructura (1/2/3): ";
        string op;
        getline(cin, op);

        //Cronometramos la busqueda
        auto t0 = chrono::high_resolution_clock::now();
        int64_t resultado = -1;

        if (op == "1") {
            resultado = c1.buscar(valor);
        } else if (op == "2") {
            resultado = c2.buscar(valor);
        } else if (op == "3") {
            resultado = c3.buscar(valor);
        } else {
            cout << "Opción inválida.\n\n";
            continue;
        }

        auto t1 = chrono::high_resolution_clock::now();
        double tiempo_ns = chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();

        if (resultado >= 0) {
            cout << "  → Encontrado en posicion " << resultado
                      << " | Tiempo: " << tiempo_ns << " ns\n\n";
        } else {
            cout << "  → No encontrado"
                      << " | Tiempo: " << tiempo_ns << " ns\n\n";
        }
    }

    cout << "Saliendo.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        mostrar_uso(argv[0]);
        return 1;
    }

    string modo(argv[1]);

    if (modo == "--benchmark") {
        modo_benchmark();
    } else if (modo == "-i") {
        if (argc < 3) {
            cerr << "Error: debe indicar la ruta del archivo CSV.\n";
            mostrar_uso(argv[0]);
            return 1;
        }
        modo_archivo(string(argv[2]));
    } else {
        cerr << "Modo desconocido: " << modo << "\n";
        mostrar_uso(argv[0]);
        return 1;
    }

    return 0;
}
