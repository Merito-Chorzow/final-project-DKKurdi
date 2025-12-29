/*
 * fsm.c — logika aplikacji (FSM + watchdog + bezpieczeństwo + telemetria)
 *
 * Rola modułu:
 * - realizuje maszynę stanów:
 *     INIT → IDLE → RUN_OPEN / RUN_CLOSED → SAFE
 * - wykonuje deterministyczny tick logiki (app_tick)
 * - nadzoruje bezpieczeństwo:
 *     - watchdog aplikacyjny (brak ticków → SAFE)
 *     - walidacja pomiaru (OOB → SAFE)
 * - integruje regulator PI oraz model obiektu (plant)
 *
 * Host vs ESP32:
 * - To jest wersja hostowa/symulacyjna (printf).
 * - Na ESP32 analogiczna logika działa z realnym czujnikiem i aktuatorami.
 */

#include <stdio.h>
#include <string.h>
#include "fsm.h"
#include "app_config.h"
#include "control.h"
#include "plant.h"

// Hostowe zależności (w embedded zwykle w kontekście aplikacji)
static pi_t    g_pi;
static plant_t g_plant;

// Fault injection: wymuszony pomiar (testy akceptacyjne)
static int   g_inject_on = 0;
static float g_inject_meas = 0.0f;

void app_inject_meas(app_t* a, float meas){
    (void)a;
    g_inject_on = 1;
    g_inject_meas = meas;
}

static float clampf(float x, float lo, float hi){
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/*
 * app_to_safe — wejście do stanu bezpiecznego.
 * W SAFE wyjście sterujące jest zerowane, a powód błędu zapamiętywany.
 */
void app_to_safe(app_t* a, safe_reason_t r, const char* msg){
    a->st = ST_SAFE;
    a->u = 0.0f;
    a->u_cmd = 0.0f;
    a->safe_reason = r;

    const char* rstr = "unknown";
    switch(r){
        case SAFE_STOP_CMD:   rstr = "stop_cmd"; break;
        case SAFE_WATCHDOG:   rstr = "watchdog_timeout"; break;
        case SAFE_SENSOR_OOB: rstr = "meas_oob"; break;
        case SAFE_BAD_STATE:  rstr = "bad_state"; break;
        default: break;
    }

    printf("[SAFE] reason=%s msg=%s ticks=%u\n",
           rstr, msg ? msg : "-", (unsigned)a->ticks);
}

/*
 * Watchdog:
 * - app_watchdog_kick() zeruje licznik „misses”
 * - app_watchdog_tick() inkrementuje misses i przy przekroczeniu progu wchodzi do SAFE
 *
 * W hostowej wersji „kick” jest robiony na początku app_tick().
 */
void app_watchdog_kick(app_t* a){
    a->wd_misses = 0;
}

void app_watchdog_tick(app_t* a){
    if (a->st == ST_SAFE) return;
    if (++a->wd_misses > WD_MAX_MISSES){
        app_to_safe(a, SAFE_WATCHDOG, "no app_tick()");
    }
}

void app_init(app_t* a){
    memset(a, 0, sizeof(*a));

    // Stan początkowy systemu
    a->st = ST_INIT;
    a->mode = MODE_OPEN;

    // Konfiguracja sterowania
    a->setpoint = 25.0f;   // domyślny SP w trybie CLOSED
    a->u_cmd = 0.0f;       // ręczne sterowanie w OPEN
    a->u = 0.0f;

    // Brak błędu na starcie
    a->safe_reason = SAFE_NONE;

    // Init regulatora i obiektu (host)
    pi_init(&g_pi, 1.0f, 0.05f, 50.0f, U_MIN, U_MAX);
    plant_init(&g_plant, 22.0f, 0.05f, 20.0f); // y0, alpha, ambient

    // Start filtracji od aktualnego pomiaru (żeby uniknąć skoku z 0.0)
    a->meas_raw  = g_inject_on ? g_inject_meas : g_plant.y;
    a->meas_filt = g_plant.y;

    // Po inicjalizacji przechodzimy do IDLE
    a->st = ST_IDLE;
}

void app_tick(app_t* a){
    a->ticks++;

    // jeśli app_tick działa, watchdog jest karmiony
    app_watchdog_kick(a);

    // "sensor" (host): bierzemy z planty
    // UWAGA: mechanizm inject jest przygotowany do testów,
    // ale tutaj meas_raw jest nadpisywane z planty.
    // Jeśli chcesz używać injection w runtime, dodamy warunek w kolejnym kroku.
    a->meas_raw = g_plant.y;

    // walidacja pomiaru (bezpieczeństwo)
    if (a->meas_raw < T_MIN || a->meas_raw > T_MAX) {
        a->sensor_err++;
        app_to_safe(a, SAFE_SENSOR_OOB, "meas out of range");
        return;
    }

    // filtr IIR (prosty filtr 1-rzędu)
    a->meas_filt = a->meas_filt + MEAS_IIR_ALPHA * (a->meas_raw - a->meas_filt);

    // FSM: wyznaczenie sterowania u
    switch(a->st){
        case ST_IDLE:
            a->u = 0.0f;
            break;

        case ST_RUN_OPEN:
            // OPEN: u pochodzi z komendy użytkownika
            a->u = clampf(a->u_cmd, U_MIN, U_MAX);
            break;

        case ST_RUN_CLOSED:
            // CLOSED: u wyliczane przez regulator PI
            a->u = pi_step(&g_pi, a->setpoint, a->meas_filt);
            break;

        case ST_SAFE:
            a->u = 0.0f;
            break;

        default:
            app_to_safe(a, SAFE_BAD_STATE, "invalid state");
            return;
    }

    // aktuator -> obiekt (symulacja odpowiedzi obiektu)
    plant_step(&g_plant, a->u);

    // telemetria co 100 ticków
    if ((a->ticks % 100) == 0) {
        printf("[TLM] t=%u st=%d mode=%d meas=%.2f sp=%.2f u=%.1f sat=%u err=%u wd=%u\n",
            (unsigned)a->ticks, (int)a->st, (int)a->mode,
            a->meas_filt, a->setpoint, a->u,
            (unsigned)g_pi.u_sat_count, (unsigned)a->sensor_err,
            (unsigned)a->wd_misses);
    }
}
