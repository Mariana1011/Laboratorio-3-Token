// Recuadro blanco para sincronizar + verificación de TOKEN (30 s)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

// ---------- Parámetros ----------
static const char* SECRET = "USAB_2025_LAB3";   // secreto compartido (igual en el MCU)
enum { TOKEN_WINDOW_S = 30 };
static const int SYNC_WHITE_MS = 800;

// ---------- Hash simple (FNV-1a 32) + mezcla ----------
static uint32_t fnv1a32(const uint8_t *data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t token_from_step(uint32_t step) {
    // token = FNV1a( SECRET || step_le ) % 1_000_000
    uint8_t buf[64];
    size_t n = 0;
    // copia SECRET
    for (const char *p = SECRET; *p && n < sizeof(buf); ++p) buf[n++] = (uint8_t)*p;
    // añade step little-endian
    if (n + 4 <= sizeof(buf)) {
        buf[n++] = (uint8_t)(step & 0xFF);
        buf[n++] = (uint8_t)((step >> 8) & 0xFF);
        buf[n++] = (uint8_t)((step >> 16) & 0xFF);
        buf[n++] = (uint8_t)((step >> 24) & 0xFF);
    }
    uint32_t h = fnv1a32(buf, n);
    return h % 1000000u; // 6 dígitos
}

// ---------- Cronómetro de alta resolución ----------
static double qpc_freq_inv = 0.0;

static void timer_init(void){
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    qpc_freq_inv = 1000.0 / (double)f.QuadPart; // ms por tick
}

static uint64_t now_ms(void){
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (uint64_t)((double)c.QuadPart * qpc_freq_inv);
}

// ---------- Ventana blanca (Win32) ----------
static const int WIN_W = 320;
static const int WIN_H = 240;
static const int WIN_X = 100;  // zona conocida en pantalla
static const int WIN_Y = 100;

static ATOM register_class(HINSTANCE h){
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = h;
    wc.lpszClassName = "LAB3_WHITE_BOX";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    return RegisterClass(&wc);
}

static HWND show_white_box(HINSTANCE h){
    HWND w = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "LAB3_WHITE_BOX","SYNC",
        WS_POPUP, WIN_X, WIN_Y, WIN_W, WIN_H,
        NULL,NULL,h,NULL);
    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
    return w;
}

static void hide_box(HWND w){
    if (w) { ShowWindow(w, SW_HIDE); DestroyWindow(w); }
}

// ---------- Programa principal ----------
int main(void){
    printf("LAB#3 TOKEN - PC\n");
    printf("Presiona ENTER para sincronizar (recuadro blanco)...\n");
    getchar();

    timer_init();

    // 1) Muestra recuadro blanco ~800 ms (pulso de sincronismo)
    HINSTANCE h = GetModuleHandle(NULL);
    register_class(h);
    HWND box = show_white_box(h);
    uint64_t t_white_start = now_ms();
    Sleep(SYNC_WHITE_MS);
    hide_box(box);

    // 2) Define t0_ms (inicio del cronometro sincronizado con el MCU)
    uint64_t t0_ms = t_white_start;
    printf("Sincronizado. t0_ms = %" PRIu64 " ms\n", t0_ms);

    // 3) Bucle: pide TOKEN y valida
    while (1){
        // muestra tiempo y cuenta regresiva de ventana
        uint64_t t_ms = now_ms();
        uint64_t elapsed_s = (t_ms - t0_ms)/1000;
        uint32_t step = (uint32_t)(elapsed_s / TOKEN_WINDOW_S);
        uint32_t remain = TOKEN_WINDOW_S - (elapsed_s % TOKEN_WINDOW_S);

        printf("\nVentana #%u (faltan %us). Ingrese token de 6 digitos (o 'q' para salir): ",
               step, remain);
        fflush(stdout);

        char buf[64]; 
        if(!fgets(buf, sizeof buf, stdin)) break;
        if(buf[0]=='q' || buf[0]=='Q') break;

        // parse token
        unsigned user_code = 0;
        if (sscanf(buf, "%u", &user_code) != 1){
            printf("Entrada no valida.\n");
            continue;
        }

        // Calcula tokens válidos en ventana actual y adyacentes (tolerancia ±1)
        uint32_t cand[3];
        cand[0] = token_from_step(step);
        cand[1] = token_from_step(step ? step-1 : 0);
        cand[2] = token_from_step(step+1);

        if (user_code==cand[0] || user_code==cand[1] || user_code==cand[2]){
            printf("TOKEN valido.\n");
        } else {
            printf("TOKEN invalido. (Actual=%06u, -1=%06u, +1=%06u)\n",
                   cand[0], cand[1], cand[2]);
        }
    }

    return 0;
}
