#pragma once
/*
 * fsm.h — stan aplikacji (FSM), tryby pracy i API sterowania
 *
 * FSM (minimalne wymagania projektu):
 *   INIT → IDLE → RUN(OPEN/CLOSED) → SAFE
 *
 * Tryby:
 * - OPEN:
 *   wyjście u pochodzi bezpośrednio z komendy (u_cmd),
 *   a aplikacja pełni rolę „przepustki” z ograniczeniami (clamp).
 *
 * - CLOSED:
 *   wyjście u pochodzi z regulatora PI na podstawie (setpoint, meas_filt).
 *
 * SAFE:
 * - stan bezpieczny (latched), do którego wchodzimy po STOP, watchdog lub błędach pomiaru.
 * - wyjścia są zerowane (u=0) i zapamiętywany jest powód.
 */

#include <stdint.h>
#include "app_config.h" // stałe: APP_TICK_MS, WD_MAX_MISSES, T_MIN/T_MAX, MEAS_IIR_ALPHA

typedef enum {
    ST_INIT = 0,
    ST_IDLE,
    ST_RUN_OPEN,
    ST_RUN_CLOSED,
    ST_SAFE
} app_state_t;

typedef enum {
    SAFE_NONE = 0,
    SAFE_STOP_CMD,
    SAFE_WATCHDOG,
    SAFE_SENSOR_OOB,
    SAFE_BAD_STATE
} safe_reason_t;

typedef enum {
    MODE_OPEN = 0,
    MODE_CLOSED = 1
} app_mode_t;

/*
 * app_t — pełny stan aplikacji (kontekst)
 * W embedded zazwyczaj trzymany jako jedna struktura w module aplikacji.
 */
typedef struct {
    // FSM + tryb
    app_state_t st;
    app_mode_t  mode;

    // Sterowanie i pomiary
    float setpoint;   // zadana wartość (CLOSED)
    float meas_raw;   // pomiar surowy (sensor / symulator)
    float meas_filt;  // pomiar filtrowany (IIR)
    float u_cmd;      // ręczne sterowanie (OPEN)
    float u;          // wyjście faktyczne (0..100)

    // Watchdog / błędy
    uint32_t ticks;       // licznik ticków logiki
    uint32_t wd_misses;   // licznik „missów” watchdog
    uint32_t sensor_err;  // licznik błędów pomiaru (np. OOB)
    safe_reason_t safe_reason;

    // Liczniki telemetrii
    uint32_t mode_changes;
} app_t;

// ===== API aplikacji =====
void app_init(app_t* a);
void app_tick(app_t* a);
void app_to_safe(app_t* a, safe_reason_t r, const char* msg);

// ===== Watchdog API =====
// Rozdzielone, bo w testach możemy symulować brak app_tick().
void app_watchdog_kick(app_t* a);
void app_watchdog_tick(app_t* a);

// ===== Fault injection (testy) =====
// Wstrzyknięcie sztucznego pomiaru (np. do wymuszenia SAFE / scenariuszy awarii).
void app_inject_meas(app_t* a, float meas);
