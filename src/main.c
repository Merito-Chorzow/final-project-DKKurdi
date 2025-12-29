/*
 * main.c — hostowy harness testowy (scenariusze akceptacyjne)
 *
 * Rola pliku:
 * - Uruchamia aplikację (FSM + regulator + symulator obiektu) w trybie hostowym
 * - Wykonuje z góry zaplanowane scenariusze testowe:
 *   A) Funkcjonalne (przełączenia trybów, setpoint)
 *   B) Jakościowe (przebieg regulacji w CLOSED)
 *   C) Awaryjne (OOB sensora, watchdog, STOP → SAFE)
 *
 * Konwencja czasu:
 * - app_tick() jest deterministycznym krokiem logiki wykonywanym co APP_TICK_MS.
 * - W tej wersji (PC) to tylko „umowny czas” – ale struktura jest taka sama jak na MCU.
 *
 * Uwaga:
 * - CLI nie „robi czasu”; CLI tylko ustawia parametry i stan FSM.
 * - Czas/sterowanie dzieją się dopiero podczas run_for_ticks().
 */

#include <stdio.h>
#include "fsm.h"
#include "cli.h"
#include "app_config.h"

/*
 * run_for_ticks()
 * Wykonuje N kroków logiki aplikacji (symulacja czasu).
 * W typowym embedded odpowiadałoby to N wywołaniom z timera / pętli RTOS.
 */
static void run_for_ticks(app_t* a, int n){
    for(int i = 0; i < n; i++){
        app_tick(a);
    }
}

/*
 * run_watchdog_only()
 * Symuluje awarię: watchdog tyka, ale logika aplikacji NIE jest wykonywana.
 * To pozwala wymusić scenariusz „brak ticków → SAFE”.
 */
static void run_watchdog_only(app_t* a, int n){
    for(int i = 0; i < n; i++){
        app_watchdog_tick(a);
    }
}

int main(void){
    app_t app;
    app_init(&app);

    printf("READY\n");

    // ============================================================
    // TEST 0: Stan początkowy + sanity check
    // ============================================================
    // Cel: potwierdzić, że system startuje i wykonuje ticki bez błędów.
    cli_handle_line(&app, "get");
    run_for_ticks(&app, 200);

    // ============================================================
    // TEST 1: Funkcjonalny — przejście do CLOSED i regulacja PI
    // ============================================================
    // Cel: przełączyć tryb na CLOSED i sprawdzić, że u pochodzi z PI,
    // a pomiar (meas_filt) dąży do setpoint.
    cli_handle_line(&app, "mode closed");
    cli_handle_line(&app, "set 25.0");

    // 600 ticków * 10 ms = 6 s (umownie)
    run_for_ticks(&app, 600);

    // Liczniki (saturacje, błędy, watchdog, przełączenia)
    cli_handle_line(&app, "stat");

    // ============================================================
    // TEST 2: Awaryjny — pomiar poza zakresem (sensor OOB) → SAFE
    // ============================================================
    // Cel: wymusić wejście w SAFE poprzez błąd walidacji pomiaru.
    printf("\n--- TEST SENSOR OOB ---\n");

    // Fault injection: ustawiamy pomiar poza dopuszczalnym zakresem
    // (wymaga, aby w app_tick() injection było respektowane).
    app_inject_meas(&app, T_MAX + 100.0f);

    // Jeden tick powinien wystarczyć do wykrycia OOB i wejścia w SAFE.
    run_for_ticks(&app, 1);
    cli_handle_line(&app, "get");

    // ============================================================
    // TEST 3: Awaryjny — watchdog timeout (brak app_tick()) → SAFE
    // ============================================================
    // Cel: zasymulować „zawieszenie” logiki aplikacji.
    printf("\n--- TEST WD: simulate missing app_tick() ---\n");

    // Watchdog tyka, ale app_tick nie jest wywoływany.
    run_watchdog_only(&app, (int)WD_MAX_MISSES + 5);

    // Po timeout watchdog powinno być SAFE.
    cli_handle_line(&app, "get");

    // ============================================================
    // TEST 4: STOP → SAFE (manualny SAFE przez komendę)
    // ============================================================
    // Cel: sprawdzić, że STOP przełącza system do SAFE.
    // Uwaga: Jeśli system już jest w SAFE (np. po testach awaryjnych),
    // to efekt będzie „idempotentny” (pozostanie SAFE).
    cli_handle_line(&app, "stop");
    run_for_ticks(&app, 50);
    cli_handle_line(&app, "get");

    return 0;
}
