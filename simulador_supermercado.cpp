#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <omp.h>
#include <memory>

using namespace std;
using namespace std::chrono;

// Estructura para representar un producto
struct Producto {
    string nombre;
    double precio;
    string categoria;
    int stock;
    int vendidos;
};

// Estructura para representar un cliente
struct Cliente {
    int id;
    vector<pair<Producto*, int>> carrito; // producto y cantidad
    double total;
    string metodoPago;
    double tiempoCompra; // en segundos
    int cantidadProductos;
};

struct ThreadStats {
    double ventasTotales = 0.0;
    int productosVendidos = 0;
    int pagosEfectivo = 0;
    int pagosTarjeta = 0;
};

// Clase principal del simulador
class SimuladorSupermercado {
private:
    map<int, Producto> inventario;
    vector<Cliente> clientes;
    mt19937 gen;
    vector<unique_ptr<mutex>> productLocks;


    // Estadísticas globales
    double ventasTotales = 0;
    int productosVendidos = 0;
    double tiempoPromedioCompra = 0;
    int pagosEfectivo = 0;
    int pagosTarjeta = 0;
    
public:
    SimuladorSupermercado() : gen(random_device{}()) {
        inicializarInventario();
    }
    
    void inicializarInventario() {
        // 50 productos organizados por categorías con precios variados
        vector<tuple<string, double, string>> productosData = {
            // Frutas y Verduras (10 productos)
            {"Manzanas (kg)", 2.50, "Frutas"},
            {"Plátanos (kg)", 1.80, "Frutas"},
            {"Naranjas (kg)", 2.20, "Frutas"},
            {"Tomates (kg)", 3.00, "Verduras"},
            {"Lechuga", 1.50, "Verduras"},
            {"Papas (kg)", 1.20, "Verduras"},
            {"Zanahorias (kg)", 1.80, "Verduras"},
            {"Cebolla (kg)", 1.50, "Verduras"},
            {"Pimientos (kg)", 3.50, "Verduras"},
            {"Aguacates", 4.00, "Frutas"},
            
            // Lácteos (8 productos)
            {"Leche (1L)", 1.20, "Lácteos"},
            {"Yogurt Natural", 2.50, "Lácteos"},
            {"Queso Fresco", 5.50, "Lácteos"},
            {"Mantequilla", 3.20, "Lácteos"},
            {"Crema", 2.80, "Lácteos"},
            {"Queso Mozzarella", 6.00, "Lácteos"},
            {"Yogurt Griego", 3.50, "Lácteos"},
            {"Leche Deslactosada", 1.80, "Lácteos"},
            
            // Carnes (8 productos)
            {"Pollo (kg)", 8.50, "Carnes"},
            {"Carne Molida (kg)", 12.00, "Carnes"},
            {"Bistec (kg)", 18.00, "Carnes"},
            {"Chuletas Cerdo (kg)", 15.00, "Carnes"},
            {"Pescado Tilapia (kg)", 10.00, "Carnes"},
            {"Salchichas", 4.50, "Carnes"},
            {"Jamón (250g)", 5.00, "Carnes"},
            {"Tocino", 7.50, "Carnes"},
            
            // Panadería (6 productos)
            {"Pan Blanco", 2.00, "Panadería"},
            {"Pan Integral", 2.50, "Panadería"},
            {"Croissants (3pz)", 3.50, "Panadería"},
            {"Tortillas (kg)", 1.50, "Panadería"},
            {"Pan Dulce", 2.80, "Panadería"},
            {"Galletas", 3.00, "Panadería"},
            
            // Bebidas (8 productos)
            {"Coca-Cola 2L", 2.50, "Bebidas"},
            {"Agua 1L", 0.80, "Bebidas"},
            {"Jugo Naranja 1L", 3.50, "Bebidas"},
            {"Cerveza (6 pack)", 8.00, "Bebidas"},
            {"Vino Tinto", 12.00, "Bebidas"},
            {"Café Molido", 6.50, "Bebidas"},
            {"Té Verde", 4.00, "Bebidas"},
            {"Bebida Energética", 3.00, "Bebidas"},
            
            // Abarrotes (10 productos)
            {"Arroz (kg)", 2.20, "Abarrotes"},
            {"Frijoles (kg)", 3.00, "Abarrotes"},
            {"Pasta (500g)", 1.80, "Abarrotes"},
            {"Aceite (1L)", 4.50, "Abarrotes"},
            {"Azúcar (kg)", 1.50, "Abarrotes"},
            {"Sal (kg)", 0.80, "Abarrotes"},
            {"Harina (kg)", 1.20, "Abarrotes"},
            {"Cereal", 5.50, "Abarrotes"},
            {"Salsa Tomate", 2.00, "Abarrotes"},
            {"Mayonesa", 3.50, "Abarrotes"}
        };
        
        for (int i = 0; i < productosData.size(); i++) {
            Producto p;
            p.nombre = get<0>(productosData[i]);
            p.precio = get<1>(productosData[i]);
            p.categoria = get<2>(productosData[i]);
            p.stock = 500 + (gen() % 1000); // Stock inicial entre 500-1500
            p.vendidos = 0;
            inventario[i] = p;
        }
        productLocks.clear();
        productLocks.reserve(inventario.size());        // ok porque unique_ptr es movible
        for (size_t i = 0; i < inventario.size(); ++i)
            productLocks.emplace_back(std::make_unique<std::mutex>());



    }
    
    Cliente simularCliente(int id) {
        Cliente cliente;
        cliente.id = id;
        cliente.total = 0;
        cliente.cantidadProductos = 0;
        
        auto inicio = high_resolution_clock::now();
        
        // Determinar tipo de comprador por probabilidad
        uniform_real_distribution<> probDist(0, 1);
        double tipoComprador = probDist(gen);
        
        int minProductos, maxProductos;
        double probProductoCaro; // Probabilidad de elegir productos caros (>5.00)
        
        if (tipoComprador < 0.20) {
            // 20% - Comprador pequeño (pocos productos, principalmente baratos)
            minProductos = 1;
            maxProductos = 5;
            probProductoCaro = 0.1;
        } else if (tipoComprador < 0.60) {
            // 40% - Comprador promedio
            minProductos = 5;
            maxProductos = 15;
            probProductoCaro = 0.3;
        } else if (tipoComprador < 0.85) {
            // 25% - Comprador familiar
            minProductos = 15;
            maxProductos = 30;
            probProductoCaro = 0.4;
        } else {
            // 15% - Comprador grande/mayorista
            minProductos = 30;
            maxProductos = 50;
            probProductoCaro = 0.5;
        }
        
        // Determinar cantidad de productos a comprar
        uniform_int_distribution<> cantDist(minProductos, maxProductos);
        int productosAComprar = cantDist(gen);
        
        // Seleccionar productos
        uniform_int_distribution<> prodDist(0, inventario.size() - 1);
        uniform_int_distribution<> cantidadDist(1, 3); // Cantidad de cada producto
        
        for (int i = 0; i < productosAComprar; i++) {
            // Decidir si elegir producto caro o barato
            bool elegirCaro = probDist(gen) < probProductoCaro;
            
            // Intentar encontrar un producto del tipo deseado
            int intentos = 0;
            int idProducto;
            do {
                idProducto = prodDist(gen);
                intentos++;
            } while (intentos < 10 && 
                     ((elegirCaro && inventario[idProducto].precio <= 5.00) ||
                      (!elegirCaro && inventario[idProducto].precio > 5.00)));
            
            // Verificar stock disponible
            if (inventario[idProducto].stock > 0) {
                int cantidad = cantidadDist(gen);
                cantidad = min(cantidad, inventario[idProducto].stock);
                
                // Agregar al carrito
                cliente.carrito.push_back({&inventario[idProducto], cantidad});
                cliente.total += inventario[idProducto].precio * cantidad;
                cliente.cantidadProductos += cantidad;
                
                // Actualizar inventario
                inventario[idProducto].stock -= cantidad;
                inventario[idProducto].vendidos += cantidad;
            }
        }
        
        // Simular tiempo de pago
        uniform_real_distribution<> tiempoPagoDist(30, 120); // 30-120 segundos
        double tiempoPago = tiempoPagoDist(gen);
        
        // Determinar método de pago (70% tarjeta, 30% efectivo)
        if (probDist(gen) < 0.7) {
            cliente.metodoPago = "Tarjeta";
            tiempoPago *= 0.8; // Pago con tarjeta es más rápido
            pagosTarjeta++;
        } else {
            cliente.metodoPago = "Efectivo";
            pagosEfectivo++;
        }
        
        auto fin = high_resolution_clock::now();
        duration<double> duracion = fin - inicio;
        
        // Tiempo total simulado (selección + pago)
        uniform_real_distribution<> tiempoSeleccionDist(180, 600); // 3-10 minutos
        cliente.tiempoCompra = tiempoSeleccionDist(gen) + tiempoPago;
        
        // Actualizar estadísticas globales
        ventasTotales += cliente.total;
        productosVendidos += cliente.cantidadProductos;
        
        return cliente;
    }

    Cliente simularCliente_parallel(int id, ThreadStats& ts, int threadId) {
        static thread_local mt19937 genThread( (unsigned)hash<string>{}(
            to_string(chrono::high_resolution_clock::now().time_since_epoch().count())
            + "|" + to_string(threadId))
        );

        Cliente cliente;
        cliente.id = id;
        cliente.total = 0;
        cliente.cantidadProductos = 0;

        auto inicio = high_resolution_clock::now();

        uniform_real_distribution<> probDist(0, 1);
        double tipoComprador = probDist(genThread);

        int minProductos, maxProductos;
        double probProductoCaro;

        if (tipoComprador < 0.20) { minProductos = 1;  maxProductos = 5;  probProductoCaro = 0.1; }
        else if (tipoComprador < 0.60) { minProductos = 5;  maxProductos = 15; probProductoCaro = 0.3; }
        else if (tipoComprador < 0.85) { minProductos = 15; maxProductos = 30; probProductoCaro = 0.4; }
        else { minProductos = 30; maxProductos = 50; probProductoCaro = 0.5; }

        uniform_int_distribution<> cantDist(minProductos, maxProductos);
        int productosAComprar = cantDist(genThread);

        uniform_int_distribution<> prodDist(0, (int)inventario.size() - 1);
        uniform_int_distribution<> cantidadDist(1, 3);

        for (int i = 0; i < productosAComprar; i++) {
            bool elegirCaro = probDist(genThread) < probProductoCaro;

            int intentos = 0, idProducto;
            do {
                idProducto = prodDist(genThread);
                intentos++;
            } while (intentos < 10 &&
                    ((elegirCaro && inventario[idProducto].precio <= 5.00) ||
                    (!elegirCaro && inventario[idProducto].precio > 5.00)));

            {
                lock_guard<mutex> g(*productLocks[idProducto]);
                if (inventario[idProducto].stock > 0) {
                    int cantidad = cantidadDist(genThread);
                    cantidad = min(cantidad, inventario[idProducto].stock);

                    cliente.carrito.push_back({ &inventario[idProducto], cantidad });

                    cliente.total += inventario[idProducto].precio * cantidad;
                    cliente.cantidadProductos += cantidad;

                    inventario[idProducto].stock   -= cantidad;
                    inventario[idProducto].vendidos += cantidad;
                }
            }
        }

        uniform_real_distribution<> tiempoPagoDist(30, 120);
        double tiempoPago = tiempoPagoDist(genThread);

        if (probDist(genThread) < 0.7) {
            cliente.metodoPago = "Tarjeta";
            tiempoPago *= 0.8;
            ts.pagosTarjeta++;
        } else {
            cliente.metodoPago = "Efectivo";
            ts.pagosEfectivo++;
        }

        auto fin = high_resolution_clock::now();
        (void)inicio; (void)fin;

        uniform_real_distribution<> tiempoSeleccionDist(180, 600);
        cliente.tiempoCompra = tiempoSeleccionDist(genThread) + tiempoPago;

        // acumular al hilo
        ts.ventasTotales     += cliente.total;
        ts.productosVendidos += cliente.cantidadProductos;

        return cliente;
    }

    
    void ejecutarSimulacion(int numClientes) {
        cout << "\n=== INICIANDO SIMULACIÓN DE SUPERMERCADO ===" << endl;
        cout << "Simulando " << numClientes << " clientes..." << endl;
        cout << "----------------------------------------" << endl;
        
        auto inicioSimulacion = high_resolution_clock::now();
        
        for (int i = 1; i <= numClientes; i++) {
            Cliente c = simularCliente(i);
            clientes.push_back(c);
            
            // Mostrar progreso cada 500 clientes
            if (i % 500 == 0) {
                cout << "Clientes procesados: " << i << "/" << numClientes << endl;
            }
        }
        
        auto finSimulacion = high_resolution_clock::now();
        duration<double> duracionTotal = finSimulacion - inicioSimulacion;
        
        cout << "\nSimulación completada en " << fixed << setprecision(2) 
             << duracionTotal.count() << " segundos" << endl;
    }
    void ejecutarSimulacionOMP(int numClientes, int numThreads = 0) {
        if (numThreads <= 0) numThreads = omp_get_max_threads();

        cout << "\n=== INICIANDO SIMULACIÓN (OpenMP) ===" << endl;
        cout << "Hilos: " << numThreads << " | Clientes: " << numClientes << endl;
        cout << "----------------------------------------" << endl;

        auto inicioSimulacion = high_resolution_clock::now();

        clientes.clear();
        clientes.resize(numClientes);

        double ventasTotales_local = 0.0;
        int productosVendidos_local = 0;
        int pagosEfectivo_local = 0;
        int pagosTarjeta_local = 0;

        #pragma omp parallel num_threads(numThreads)
        {
            int tid = omp_get_thread_num();
            ThreadStats ts;

            #pragma omp for schedule(static)
            for (int i = 1; i <= numClientes; i++) {
                Cliente c = simularCliente_parallel(i, ts, tid);
                clientes[i-1] = std::move(c);
            }

            #pragma omp atomic
            ventasTotales_local += ts.ventasTotales;
            #pragma omp atomic
            productosVendidos_local += ts.productosVendidos;
            #pragma omp atomic
            pagosEfectivo_local += ts.pagosEfectivo;
            #pragma omp atomic
            pagosTarjeta_local += ts.pagosTarjeta;
        }

        auto finSimulacion = high_resolution_clock::now();
        duration<double> duracionTotal = finSimulacion - inicioSimulacion;

        // Volcar a los miembros globales de la clase
        ventasTotales     += ventasTotales_local;
        productosVendidos += productosVendidos_local;
        pagosEfectivo     += pagosEfectivo_local;
        pagosTarjeta      += pagosTarjeta_local;

        cout << "\nSimulación completada en " << fixed << setprecision(2)
            << duracionTotal.count() << " segundos" << endl;
    }

    
    void mostrarEstadisticas() {
        cout << "\n\n========================================" << endl;
        cout << "     ESTADÍSTICAS DE LA SIMULACIÓN      " << endl;
        cout << "========================================" << endl;
        
        // Calcular promedios
        double promedioCompra = ventasTotales / clientes.size();
        double tiempoTotal = 0;
        for (const auto& c : clientes) {
            tiempoTotal += c.tiempoCompra;
        }
        tiempoPromedioCompra = tiempoTotal / clientes.size();
        
        cout << "\n--- VENTAS ---" << endl;
        cout << "Total de clientes: " << clientes.size() << endl;
        cout << "Ventas totales: $" << fixed << setprecision(2) << ventasTotales << endl;
        cout << "Promedio por cliente: $" << promedioCompra << endl;
        cout << "Productos vendidos: " << productosVendidos << endl;
        cout << "Promedio productos/cliente: " << (double)productosVendidos/clientes.size() << endl;
        
        cout << "\n--- MÉTODOS DE PAGO ---" << endl;
        cout << "Pagos con tarjeta: " << pagosTarjeta 
             << " (" << (pagosTarjeta*100.0/clientes.size()) << "%)" << endl;
        cout << "Pagos en efectivo: " << pagosEfectivo 
             << " (" << (pagosEfectivo*100.0/clientes.size()) << "%)" << endl;
        
        cout << "\n--- TIEMPOS ---" << endl;
        cout << "Tiempo promedio de compra: " << fixed << setprecision(1) 
             << tiempoPromedioCompra << " segundos (" 
             << tiempoPromedioCompra/60.0 << " minutos)" << endl;
        
        // Top 10 productos más vendidos
        cout << "\n--- TOP 10 PRODUCTOS MÁS VENDIDOS ---" << endl;
        vector<pair<string, int>> topProductos;
        for (map<int, Producto>::const_iterator it = inventario.begin(); it != inventario.end(); ++it) {
            if (it->second.vendidos > 0) {
                topProductos.push_back(make_pair(it->second.nombre, it->second.vendidos));
            }
        }
        
        sort(topProductos.begin(), topProductos.end(), 
             [](const pair<string, int>& a, const pair<string, int>& b) { 
                 return a.second > b.second; 
             });
        
        for (int i = 0; i < min(10, (int)topProductos.size()); i++) {
            cout << setw(2) << (i+1) << ". " << left << setw(25) 
                 << topProductos[i].first << " - " 
                 << topProductos[i].second << " unidades" << endl;
        }
        
        // Estadísticas por categoría
        cout << "\n--- VENTAS POR CATEGORÍA ---" << endl;
        map<string, double> ventasPorCategoria;
        map<string, int> productosPorCategoria;
        
        for (const auto& cliente : clientes) {
            for (size_t j = 0; j < cliente.carrito.size(); j++) {
                Producto* prod = cliente.carrito[j].first;
                int cant = cliente.carrito[j].second;
                ventasPorCategoria[prod->categoria] += prod->precio * cant;
                productosPorCategoria[prod->categoria] += cant;
            }
        }
        
        for (map<string, double>::const_iterator it = ventasPorCategoria.begin(); 
             it != ventasPorCategoria.end(); ++it) {
            cout << left << setw(15) << it->first << ": $" << fixed << setprecision(2) 
                 << setw(10) << it->second << " (" << productosPorCategoria[it->first] 
                 << " productos)" << endl;
        }
        
        // Distribución de tipos de compradores
        cout << "\n--- DISTRIBUCIÓN DE COMPRADORES ---" << endl;
        int pequenos = 0, medianos = 0, grandes = 0, mayoristas = 0;
        
        for (const auto& c : clientes) {
            if (c.cantidadProductos <= 5) pequenos++;
            else if (c.cantidadProductos <= 15) medianos++;
            else if (c.cantidadProductos <= 30) grandes++;
            else mayoristas++;
        }
        
        cout << "Compradores pequeños (1-5 productos): " << pequenos 
             << " (" << (pequenos*100.0/clientes.size()) << "%)" << endl;
        cout << "Compradores medianos (6-15 productos): " << medianos 
             << " (" << (medianos*100.0/clientes.size()) << "%)" << endl;
        cout << "Compradores grandes (16-30 productos): " << grandes 
             << " (" << (grandes*100.0/clientes.size()) << "%)" << endl;
        cout << "Compradores mayoristas (>30 productos): " << mayoristas 
             << " (" << (mayoristas*100.0/clientes.size()) << "%)" << endl;
        
        cout << "\n========================================" << endl;
    }
    
    void mostrarInventarioFinal() {
        cout << "\n--- ESTADO FINAL DEL INVENTARIO ---" << endl;
        cout << "Productos con stock bajo (<100 unidades):" << endl;
        
        for (map<int, Producto>::const_iterator it = inventario.begin(); it != inventario.end(); ++it) {
            if (it->second.stock < 100) {
                cout << "- " << it->second.nombre << ": " << it->second.stock 
                     << " unidades restantes" << endl;
            }
        }
    }
};

int main() {
    // Configuración inicial
    cout << "╔════════════════════════════════════════╗" << endl;
    cout << "║   SIMULADOR DE SUPERMERCADO v1.0      ║" << endl;
    cout << "╚════════════════════════════════════════╝" << endl;
    
    SimuladorSupermercado simulador;
    
    // Solicitar número de clientes
    int numClientes;
    cout << "\n¿Cuántos clientes desea simular?" << endl;
    cout << "Recomendado: 3000-4000 para un supermercado grande" << endl;
    cout << "Ingrese el número: ";
    cin >> numClientes;
    
    // Validar entrada
    if (numClientes < 1) {
        cout << "Número inválido. Usando valor por defecto: 3500" << endl;
        numClientes = 3500;
    }
    
    // Ejecutar simulación
    int modo;
    cout << "\nModo de simulación: 1) Secuencial  2) Paralela (OpenMP)\n";
    cout << "Ingrese 1 o 2: ";
    cin >> modo;

    if (modo == 2) {
        int hilos;
        cout << "¿Cuántos hilos? (0 = max del sistema): ";
        cin >> hilos;
        simulador.ejecutarSimulacionOMP(numClientes, hilos);
    } else {
        simulador.ejecutarSimulacion(numClientes); // tu versión original
    }
    
    // Mostrar resultados
    simulador.mostrarEstadisticas();
    simulador.mostrarInventarioFinal();
    
    cout << "\n=== SIMULACIÓN FINALIZADA ===" << endl;
    cout << "Presione Enter para salir...";
    cin.ignore();
    cin.get();
    
    return 0;
}