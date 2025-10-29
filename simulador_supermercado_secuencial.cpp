#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

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

// Clase principal del simulador
class SimuladorSupermercado {
private:
    map<int, Producto> inventario;
    vector<Cliente> clientes;
    mt19937 gen;
    
    // Estadisticas globales
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
        // 50 productos organizados por categorias con precios variados
        vector<tuple<string, double, string>> productosData = {
            // Frutas y Verduras (10 productos)
            {"Manzanas (kg)", 2.50, "Frutas"},
            {"Platanos (kg)", 1.80, "Frutas"},
            {"Naranjas (kg)", 2.20, "Frutas"},
            {"Tomates (kg)", 3.00, "Verduras"},
            {"Lechuga", 1.50, "Verduras"},
            {"Papas (kg)", 1.20, "Verduras"},
            {"Zanahorias (kg)", 1.80, "Verduras"},
            {"Cebolla (kg)", 1.50, "Verduras"},
            {"Pimientos (kg)", 3.50, "Verduras"},
            {"Aguacates", 4.00, "Frutas"},
            
            // Lacteos (8 productos)
            {"Leche (1L)", 1.20, "Lacteos"},
            {"Yogurt Natural", 2.50, "Lacteos"},
            {"Queso Fresco", 5.50, "Lacteos"},
            {"Mantequilla", 3.20, "Lacteos"},
            {"Crema", 2.80, "Lacteos"},
            {"Queso Mozzarella", 6.00, "Lacteos"},
            {"Yogurt Griego", 3.50, "Lacteos"},
            {"Leche Deslactosada", 1.80, "Lacteos"},
            
            // Carnes (8 productos)
            {"Pollo (kg)", 8.50, "Carnes"},
            {"Carne Molida (kg)", 12.00, "Carnes"},
            {"Bistec (kg)", 18.00, "Carnes"},
            {"Chuletas Cerdo (kg)", 15.00, "Carnes"},
            {"Pescado Tilapia (kg)", 10.00, "Carnes"},
            {"Salchichas", 4.50, "Carnes"},
            {"Jamon (250g)", 5.00, "Carnes"},
            {"Tocino", 7.50, "Carnes"},
            
            // Panaderia (6 productos)
            {"Pan Blanco", 2.00, "Panaderia"},
            {"Pan Integral", 2.50, "Panaderia"},
            {"Croissants (3pz)", 3.50, "Panaderia"},
            {"Tortillas (kg)", 1.50, "Panaderia"},
            {"Pan Dulce", 2.80, "Panaderia"},
            {"Galletas", 3.00, "Panaderia"},
            
            // Bebidas (8 productos)
            {"Coca-Cola 2L", 2.50, "Bebidas"},
            {"Agua 1L", 0.80, "Bebidas"},
            {"Jugo Naranja 1L", 3.50, "Bebidas"},
            {"Cerveza (6 pack)", 8.00, "Bebidas"},
            {"Vino Tinto", 12.00, "Bebidas"},
            {"Cafe Molido", 6.50, "Bebidas"},
            {"Te Verde", 4.00, "Bebidas"},
            {"Bebida Energetica", 3.00, "Bebidas"},
            
            // Abarrotes (10 productos)
            {"Arroz (kg)", 2.20, "Abarrotes"},
            {"Frijoles (kg)", 3.00, "Abarrotes"},
            {"Pasta (500g)", 1.80, "Abarrotes"},
            {"Aceite (1L)", 4.50, "Abarrotes"},
            {"Azucar (kg)", 1.50, "Abarrotes"},
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
            // Stock ilimitado simulado con un numero muy grande
            p.stock = 999999999; // Stock practicamente ilimitado
            p.vendidos = 0;
            inventario[i] = p;
        }
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
            // 20% - Comprador pequeno (pocos productos, principalmente baratos)
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
        
        // Determinar metodo de pago (70% tarjeta, 30% efectivo)
        if (probDist(gen) < 0.7) {
            cliente.metodoPago = "Tarjeta";
            tiempoPago *= 0.8; // Pago con tarjeta es mas rapido
            pagosTarjeta++;
        } else {
            cliente.metodoPago = "Efectivo";
            pagosEfectivo++;
        }
        
        auto fin = high_resolution_clock::now();
        duration<double> duracion = fin - inicio;
        
        // Tiempo total simulado (seleccion + pago)
        uniform_real_distribution<> tiempoSeleccionDist(180, 600); // 3-10 minutos
        cliente.tiempoCompra = tiempoSeleccionDist(gen) + tiempoPago;
        
        // Actualizar estadisticas globales
        ventasTotales += cliente.total;
        productosVendidos += cliente.cantidadProductos;
        
        return cliente;
    }
    
    void ejecutarSimulacion(int numClientes) {
        cout << "\n=== INICIANDO SIMULACION DE SUPERMERCADO ===" << endl;
        cout << "Simulando " << numClientes << " clientes..." << endl;
        cout << "----------------------------------------" << endl;
        
        auto inicioSimulacion = high_resolution_clock::now();
        
        for (int i = 1; i <= numClientes; i++) {
            Cliente c = simularCliente(i);
            clientes.push_back(c);
            
            // Mostrar progreso cada 500 clientes (o cada 10000 si son muchos)
            int intervalo = (numClientes > 10000) ? 10000 : 500;
            if (i % intervalo == 0) {
                cout << "Clientes procesados: " << i << "/" << numClientes << endl;
            }
        }
        
        auto finSimulacion = high_resolution_clock::now();
        duration<double> duracionTotal = finSimulacion - inicioSimulacion;
        
        cout << "\nSimulacion completada en " << fixed << setprecision(2) 
             << duracionTotal.count() << " segundos" << endl;
    }
    
    void mostrarEstadisticas() {
        cout << "\n\n========================================" << endl;
        cout << "     ESTADISTICAS DE LA SIMULACION      " << endl;
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
        
        cout << "\n--- METODOS DE PAGO ---" << endl;
        cout << "Pagos con tarjeta: " << pagosTarjeta 
             << " (" << (pagosTarjeta*100.0/clientes.size()) << "%)" << endl;
        cout << "Pagos en efectivo: " << pagosEfectivo 
             << " (" << (pagosEfectivo*100.0/clientes.size()) << "%)" << endl;
        
        cout << "\n--- TIEMPOS ---" << endl;
        cout << "Tiempo promedio de compra: " << fixed << setprecision(1) 
             << tiempoPromedioCompra << " segundos (" 
             << tiempoPromedioCompra/60.0 << " minutos)" << endl;
        
        // Top 10 productos mas vendidos
        cout << "\n--- TOP 10 PRODUCTOS MAS VENDIDOS ---" << endl;
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
        
        // Estadisticas por categoria
        cout << "\n--- VENTAS POR CATEGORIA ---" << endl;
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
        
        // Distribucion de tipos de compradores
        cout << "\n--- DISTRIBUCION DE COMPRADORES ---" << endl;
        int pequenos = 0, medianos = 0, grandes = 0, mayoristas = 0;
        
        for (const auto& c : clientes) {
            if (c.cantidadProductos <= 5) pequenos++;
            else if (c.cantidadProductos <= 15) medianos++;
            else if (c.cantidadProductos <= 30) grandes++;
            else mayoristas++;
        }
        
        cout << "Compradores pequenos (1-5 productos): " << pequenos 
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
        cout << "\n--- RESUMEN DE INVENTARIO ---" << endl;
        
        // Mostrar total de unidades vendidas
        int totalUnidades = 0;
        double ingresoTotal = 0;
        
        for (map<int, Producto>::const_iterator it = inventario.begin(); it != inventario.end(); ++it) {
            totalUnidades += it->second.vendidos;
            ingresoTotal += it->second.vendidos * it->second.precio;
        }
        
        cout << "Total de unidades vendidas: " << totalUnidades << endl;
        cout << "Ingresos totales calculados: $" << fixed << setprecision(2) << ingresoTotal << endl;
        
        // Mostrar productos menos vendidos (puede indicar falta de demanda)
        cout << "\nProductos con menor demanda (<500 unidades vendidas):" << endl;
        for (map<int, Producto>::const_iterator it = inventario.begin(); it != inventario.end(); ++it) {
            if (it->second.vendidos < 500 && it->second.vendidos > 0) {
                cout << "- " << it->second.nombre << ": " << it->second.vendidos 
                     << " unidades vendidas" << endl;
            }
        }
    }
};

int main() {
    // Configuracion inicial
    cout << "========================================" << endl;
    cout << "    SIMULADOR DE SUPERMERCADO v1.0     " << endl;
    cout << "========================================" << endl;
    
    SimuladorSupermercado simulador;
    
    // Solicitar numero de clientes
    int numClientes;
    cout << "\nCuantos clientes desea simular?" << endl;
    cout << "Recomendado: 3000-4000 para un supermercado grande" << endl;
    cout << "Ingrese el numero: ";
    cin >> numClientes;
    
    // Validar entrada
    if (numClientes < 1) {
        cout << "Numero invalido. Usando valor por defecto: 3500" << endl;
        numClientes = 3500;
    }
    
    // Ejecutar simulacion
    simulador.ejecutarSimulacion(numClientes);
    
    // Mostrar resultados
    simulador.mostrarEstadisticas();
    simulador.mostrarInventarioFinal();
    
    cout << "\n=== SIMULACION FINALIZADA ===" << endl;
    cout << "Presione Enter para salir...";
    cin.ignore();
    cin.get();
    
    return 0;
}