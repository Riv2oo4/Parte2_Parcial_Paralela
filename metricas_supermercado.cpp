#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <regex>
#include <iomanip>
#include <map>
#include <algorithm>
#include <thread>

using namespace std;
using namespace std::chrono;

static const string EXE_UNIFICADO = "simulador_supermercado.exe";
static const string EXE_SECUENCIAL_PURO = "simulador_supermercado_secuencial.exe";
static const vector<int> CLIENTES = {1000, 2000, 4000, 8000};
static const vector<int> HILOS = {2, 4, 8, 0};
static const int REPETICIONES = 3;
static const string CSV_SALIDA  = "metricas_resultados.csv";
static const string HTML_SALIDA = "metricas_reporte.html";

struct Resultado {
    int clientes = 0;
    string modo;
    int hilos = 1;
    double tiempo_s = 0.0;
};

static bool run_process_with_input(const string& exePath,
                                   const string& stdinText,
                                   string& stdoutText,
                                   double& wallSeconds)
{
    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
    HANDLE hInRead=nullptr, hInWrite=nullptr, hOutRead=nullptr, hOutWrite=nullptr;
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) return false;
    if (!SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0)) return false;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) return false;
    if (!SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0)) return false;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdInput  = hInRead;
    si.hStdOutput = hOutWrite;
    si.hStdError  = hOutWrite;
    si.dwFlags    = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};
    string cmd = "\"" + exePath + "\"";
    vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');
    auto t0 = high_resolution_clock::now();
    BOOL ok = CreateProcessA(
        NULL, cmdline.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hInRead);
    CloseHandle(hOutWrite);
    if (!ok) {
        CloseHandle(hInWrite);
        CloseHandle(hOutRead);
        return false;
    }
    string norm = stdinText;
    for (size_t i = 0; i < norm.size(); ++i) {
        if (norm[i] == '\n') {
            if (i == 0 || norm[i-1] != '\r') {
                norm.insert(i, "\r");
                ++i;
            }
        }
    }
    DWORD written=0;
    if (!stdinText.empty()) {
        WriteFile(hInWrite, norm.data(), (DWORD)norm.size(), &written, NULL);
    }
    CloseHandle(hInWrite);
    const DWORD BUFSZ = 1<<12;
    char buf[BUFSZ];
    DWORD read=0;
    stdoutText.clear();
    for (;;) {
        BOOL okr = ReadFile(hOutRead, buf, BUFSZ, &read, NULL);
        if (!okr || read == 0) break;
        stdoutText.append(buf, buf + read);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    auto t1 = high_resolution_clock::now();
    wallSeconds = duration<double>(t1 - t0).count();
    CloseHandle(hOutRead);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static double parse_seconds_from_output(const string& out)
{
    std::regex rgx(
        "(Simulaci[oó]n\\s+completada\\s+en\\s+)([0-9]+(?:[\\.,][0-9]+)?)\\s+segundos",
        std::regex::icase);
    std::smatch m;
    if (std::regex_search(out, m, rgx) && m.size() >= 3) {
        string num = m[2].str();
        for (char& c : num) if (c == ',') c = '.';
        try {
            return std::stod(num);
        } catch (...) {}
    }
    return -1.0;
}

static string make_stdin_unificado(int clientes, int modo, int hilos)
{
    ostringstream ss;
    ss << clientes << "\n";
    ss << modo << "\n";
    if (modo == 2) ss << hilos << "\n";
    ss << "\n";
    return ss.str();
}

static string make_stdin_secuencial_puro(int clientes)
{
    ostringstream ss;
    ss << clientes << "\n";
    ss << "\n";
    return ss.str();
}

int main()
{
    ios::sync_with_stdio(false);
    cout << "=== METRICAS SUPERMERCADO (Windows) ===" << endl;
    vector<Resultado> resultados;
    {
        cout << "\n>> Ejecutando con: " << EXE_UNIFICADO << endl;
        for (int clientes : CLIENTES) {
            {
                double acumulado = 0.0;
                for (int r = 0; r < REPETICIONES; ++r) {
                    string out; double wall=0;
                    string in = make_stdin_unificado(clientes, 1, 1);
                    bool ok = run_process_with_input(EXE_UNIFICADO, in, out, wall);
                    if (!ok) { cerr << "Error lanzando " << EXE_UNIFICADO << " (sec)\n"; return 1; }
                    double t = parse_seconds_from_output(out);
                    if (t < 0) t = wall;
                    acumulado += t;
                }
                Resultado res;
                res.clientes = clientes;
                res.modo = "sec";
                res.hilos = 1;
                res.tiempo_s = acumulado / REPETICIONES;
                resultados.push_back(res);
                cout << "SEC  | N=" << setw(5) << clientes << "  t=" << fixed << setprecision(3) << res.tiempo_s << " s" << endl;
            }
            for (int h : HILOS) {
                double acumulado = 0.0;
                for (int r = 0; r < REPETICIONES; ++r) {
                    string out; double wall=0;
                    string in = make_stdin_unificado(clientes, 2, h);
                    bool ok = run_process_with_input(EXE_UNIFICADO, in, out, wall);
                    if (!ok) { cerr << "Error lanzando " << EXE_UNIFICADO << " (par)\n"; return 1; }
                    double t = parse_seconds_from_output(out);
                    if (t < 0) t = wall;
                    acumulado += t;
                }
                Resultado res;
                res.clientes = clientes;
                res.modo = "par";
                res.hilos = (h == 0 ? max(2u, thread::hardware_concurrency()) : h);
                res.tiempo_s = acumulado / REPETICIONES;
                resultados.push_back(res);
                cout << "PAR  | N=" << setw(5) << clientes << "  H=" << setw(2) << (h==0?res.hilos:h)
                     << "  t=" << fixed << setprecision(3) << res.tiempo_s << " s" << endl;
            }
        }
    }
    if (!EXE_SECUENCIAL_PURO.empty()) {
        cout << "\n>> Ejecutando con (secuencial puro): " << EXE_SECUENCIAL_PURO << endl;
        for (int clientes : CLIENTES) {
            double acumulado = 0.0;
            for (int r = 0; r < REPETICIONES; ++r) {
                string out; double wall=0;
                string in = make_stdin_secuencial_puro(clientes);
                bool ok = run_process_with_input(EXE_SECUENCIAL_PURO, in, out, wall);
                if (!ok) { cerr << "Error lanzando " << EXE_SECUENCIAL_PURO << "\n"; return 1; }
                double t = parse_seconds_from_output(out);
                if (t < 0) t = wall;
                acumulado += t;
            }
            Resultado res;
            res.clientes = clientes;
            res.modo = "sec_puro";
            res.hilos = 1;
            res.tiempo_s = acumulado / REPETICIONES;
            resultados.push_back(res);
            cout << "SEC* | N=" << setw(5) << clientes << "  t=" << fixed << setprecision(3) << res.tiempo_s << " s" << endl;
        }
    }
    map<int,double> baseSec;
    for (auto& r : resultados) {
        if (r.modo == "sec") baseSec[r.clientes] = r.tiempo_s;
    }
    {
        ofstream f(CSV_SALIDA);
        f << "clientes,modo,hilos,tiempo_s,speedup,eficiencia\n";
        for (auto& r : resultados) {
            double speedup = (baseSec.count(r.clientes) && r.tiempo_s > 0)
                             ? (baseSec[r.clientes] / r.tiempo_s) : 0.0;
            double eff = (r.modo == "par" && r.hilos > 1) ? (speedup / r.hilos) : (r.modo=="sec"?1.0:0.0);
            f << r.clientes << "," << r.modo << "," << r.hilos << ","
              << fixed << setprecision(6) << r.tiempo_s << ","
              << fixed << setprecision(6) << speedup << ","
              << fixed << setprecision(6) << eff << "\n";
        }
    }
    cout << "\nCSV generado: " << CSV_SALIDA << endl;
    auto html_escape = [](const string& s)->string {
        string o; o.reserve(s.size()*1.1);
        for (char c : s) {
            switch(c) {
                case '&': o += "&amp;"; break;
                case '<': o += "&lt;"; break;
                case '>': o += "&gt;"; break;
                case '"': o += "&quot;"; break;
                case '\'': o += "&#39;"; break;
                default: o += c;
            }
        }
        return o;
    };
    map<int, vector<Resultado>> porN;
    for (auto& r : resultados) porN[r.clientes].push_back(r);
    double tmax = 0.0;
    for (auto& r : resultados) tmax = max(tmax, r.tiempo_s);
    ofstream h(HTML_SALIDA);
    h << "<!doctype html><meta charset='utf-8'>\n";
    h << "<title>Métricas Supermercado</title>\n";
    h << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px}"
         ".card{border:1px solid #ddd;border-radius:10px;padding:16px;margin-bottom:20px}"
         "table{border-collapse:collapse;width:100%}th,td{border:1px solid #ddd;padding:6px;text-align:center}"
         "th{background:#f7f7f7} .tag{display:inline-block;padding:2px 8px;border-radius:12px;background:#eee;margin-left:6px;font-size:12px}"
         "</style>\n";
    h << "<h1>Métricas de ejecución</h1>\n";
    h << "<p>Este reporte compara tiempos de ejecución secuencial vs paralelo (OpenMP) y calcula speedup/eficiencia. "
         "Datos exportados también a <b>" << html_escape(CSV_SALIDA) << "</b>.</p>\n";
    h << "<div class='card'><h2>Resumen (promedios de " << REPETICIONES << " corridas)</h2>\n";
    h << "<table><tr><th>Clientes</th><th>Modo</th><th>Hilos</th><th>Tiempo (s)</th><th>Speedup</th><th>Eficiencia</th></tr>\n";
    for (auto& r : resultados) {
        double speedup = (baseSec.count(r.clientes) && r.tiempo_s > 0)
                         ? (baseSec[r.clientes] / r.tiempo_s) : 0.0;
        double eff = (r.modo == "par" && r.hilos > 1) ? (speedup / r.hilos) : (r.modo=="sec"?1.0:0.0);
        h << "<tr><td>" << r.clientes << "</td>"
          << "<td>" << r.modo << "</td>"
          << "<td>" << r.hilos << "</td>"
          << "<td>" << fixed << setprecision(3) << r.tiempo_s << "</td>"
          << "<td>" << fixed << setprecision(3) << speedup << "</td>"
          << "<td>" << fixed << setprecision(3) << eff << "</td></tr>\n";
    }
    h << "</table></div>\n";
    for (auto& kv : porN) {
        int n = kv.first;
        auto filas = kv.second;
        sort(filas.begin(), filas.end(), [](const Resultado& a, const Resultado& b){
            if (a.modo == b.modo) return a.hilos < b.hilos;
            if (a.modo == "sec") return true;
            if (b.modo == "sec") return false;
            if (a.modo == "sec_puro" && b.modo != "sec") return false;
            if (b.modo == "sec_puro" && a.modo != "sec") return true;
            return a.modo < b.modo;
        });
        h << "<div class='card'><h2>N = " << n << " <span class='tag'>barras</span></h2>";
        int W = 760, H = 30 * (int)filas.size() + 60;
        int left = 180, right = 20, top = 20, barH = 24, gap = 6;
        h << "<svg width='"<<W<<"' height='"<<H<<"' viewBox='0 0 "<<W<<" "<<H<<"'>\n";
        h << "<line x1='"<<left<<"' y1='"<<H-top<<"' x2='"<<(W-right)<<"' y2='"<<H-top<<"' stroke='#999'/>\n";
        int i = 0;
        for (auto& r : filas) {
            double frac = (tmax>0? (r.tiempo_s / tmax) : 0.0);
            int barW = (int)((W-left-right)*frac);
            int y = top + i*(barH+gap);
            ostringstream lbl;
            lbl << r.modo;
            if (r.modo == "par") lbl << " ("<< r.hilos <<"h)";
            if (r.modo == "sec_puro") lbl << " (*)";
            h << "<text x='"<< (left-10) <<"' y='"<< (y+barH-6) <<"' text-anchor='end' font-size='12'>"
              << lbl.str() << "</text>\n";
            h << "<rect x='"<<left<<"' y='"<<y<<"' width='"<<barW<<"' height='"<<barH<<"' fill='#4e79a7'/>\n";
            ostringstream val;
            val << fixed << setprecision(3) << r.tiempo_s << " s";
            h << "<text x='"<< (left+barW+6) <<"' y='"<< (y+barH-6) <<"' font-size='12'>"<< val.str() <<"</text>\n";
            ++i;
        }
        h << "<text x='"<< ((left+W-right)/2) <<"' y='"<< (H-4) <<"' text-anchor='middle' font-size='12' fill='#555'>Tiempo (s) — escalado al máximo observado</text>\n";
        h << "</svg></div>\n";
    }
    h << "<p style='color:#666;font-size:13px'>(*) “sec_puro” corresponde al binario secuencial independiente si se configuró ruta. "
         "Las comparaciones de speedup usan como base el modo “sec” del ejecutable unificado para consistencia.</p>\n";
    h.close();
    cout << "HTML generado: " << HTML_SALIDA << endl;
    cout << "\nListo. Abre " << HTML_SALIDA << " para ver las gráficas.\n";
    return 0;
}
