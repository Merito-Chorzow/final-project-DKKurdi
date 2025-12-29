#pragma once
/*
 * app_config.h — centralna konfiguracja systemu (host/embedded)
 *
 * Rola:
 * - Trzyma wszystkie stałe konfiguracyjne w jednym miejscu, aby:
 *   (a) ułatwić strojenie układu,
 *   (b) unikać „magicznych liczb” w kodzie,
 *   (c) zapewnić spójność w testach akceptacyjnych.
 *
 * Uwagi:
 * - W wersji hostowej „tick” jest umowny (bez realnego timera),
 *   ale architektura pozostaje zgodna z embedded: deterministyczny krok + harmonogram.
 */

// Okres kroku logiki (tick) w milisekundach.
// 10 ms => 100 Hz (często spotykane w pętlach sterowania/telemetrii)
#define APP_TICK_MS          10

// Zakres wyjścia aktuatora (np. PWM / procent wysterowania)
#define U_MIN               0.0f
#define U_MAX             100.0f

// Bezpieczny zakres temperatury (walidacja pomiaru, przejście do SAFE przy OOB)
#define T_MIN             -20.0f
#define T_MAX              80.0f

// Watchdog aplikacyjny: ile ticków bez poprawnego kroku logiki dopuszczamy.
// WD_MAX_MISSES * APP_TICK_MS = czas do SAFE.
// Domyślnie: 50 * 10ms = 500ms
#define WD_MAX_MISSES        50

// Filtr pomiaru (IIR 1-rzędu): meas_filt = meas_filt + alpha*(meas_raw - meas_filt)
// alpha ∈ (0..1). Większe => szybciej reaguje, ale mniej filtruje szum.
#define MEAS_IIR_ALPHA     0.20f
