#  Makefile

CXX      = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
TARGET   = main
SRCS     = main.cpp

# Archivos de cabecera hpp
HEADERS  = caso1_explicito.hpp caso2_gapcoding.hpp caso3_pef.hpp


# Regla principal: compilar el ejecutable

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)
	@echo "Compilación exitosa: ./$(TARGET)"


#  Limpiar archivos generados
clean:
	rm -f $(TARGET) resultados_benchmark.csv

#Ejecutar el modo benchmark

benchmark: $(TARGET)
	./$(TARGET) --benchmark

.PHONY: all clean benchmark
