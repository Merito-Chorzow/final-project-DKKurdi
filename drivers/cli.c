/*
 * cli.c — prosty interfejs tekstowy (UART/STDOUT) dla aplikacji sterowania
 *
 * Cel modułu:
 * - Dostarcza minimalny protokół CLI do sterowania aplikacją (FSM + regulator)
 * - Umożliwia testy akceptacyjne bez GUI: sekwencje komend i logi na stdout
 *
 * Kontrakt komend:
 * - get
 *     Wypisuje bieżący stan systemu: state, mode, meas (filtrowany), setpoint, u, ticks
 *
 * - mode open | mode closed
 *     Przełącza tryb pracy:
 *       OPEN   : u (wyjście) pochodzi z komendy "out"
 *       CLOSED : u pochodzi z regulatora PI na podstawie setpoint i meas
 *
 * - set <float>
 *     Ustawia setpoint (wartość zadaną) dla trybu CLOSED
 *
 * - out <float>
 *     Ustawia u_cmd (ręczne sterowanie) dla trybu OPEN
 *
 * - run
 *     Przechodzi do RUN_* zależnie od aktualnego mode
 *
 * - stop
 *     Wymusza przejście do SAFE (powód: STOP_CMD)
 *
 * - stat
 *     Wypisuje liczniki telemetrii: ticks, mode_changes, sensor_err, wd_misses
 *
 * Uwagi:
 * - Parser jest celowo prosty (strcmp/strncmp), aby pozostać deterministycznym
 *   i łatwym do przeniesienia na embedded (bez ciężkiego frameworka).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cli.h"
#include "fsm.h"

// Ujednolicone formatowanie odpowiedzi CLI (łatwe do grep / testów)
static void ok(const char* s){  printf("OK %s\n", s); }
static void err(const char* s){ printf("ERR %s\n", s); }

/*
 * cli_handle_line(app, line)
 * - line: pojedyncza linia komendy bez '\n' (np. "mode open")
 * - Funkcja nie modyfikuje czasu; jedynie zmienia stan/parametry.
 * - Tick logiki wykonywany jest osobno (app_tick).
 */
void cli_handle_line(app_t* a, const char* line){
    // 1) GET — snapshot stanu (dla debug/testów)
    if (strcmp(line, "get") == 0){
        printf("state=%d mode=%d meas=%.2f sp=%.2f u=%.1f ticks=%u\n",
               (int)a->st, (int)a->mode,
               a->meas_filt, a->setpoint, a->u, (unsigned)a->ticks);
        return;
    }

    // 2) MODE — przełączanie OPEN/CLOSED
    // Uwaga: w tym MVP bez „bumpless transfer”; przełączenie jest natychmiastowe.
    if (strncmp(line, "mode ", 5) == 0){
        if (strcmp(line + 5, "open") == 0){
            a->mode = MODE_OPEN;
            a->st = ST_RUN_OPEN;
            a->mode_changes++;
            ok("mode=OPEN");
            return;
        }
        if (strcmp(line + 5, "closed") == 0){
            a->mode = MODE_CLOSED;
            a->st = ST_RUN_CLOSED;
            a->mode_changes++;
            ok("mode=CLOSED");
            return;
        }
        err("bad_mode");
        return;
    }

    // 3) SET — setpoint dla CLOSED
    if (strncmp(line, "set ", 4) == 0){
        // strtof ignoruje błędy formatu — w wersji produkcyjnej warto sprawdzać endptr.
        a->setpoint = strtof(line + 4, NULL);
        ok("setpoint");
        return;
    }

    // 4) OUT — ręczne sterowanie u_cmd dla OPEN
    if (strncmp(line, "out ", 4) == 0){
        a->u_cmd = strtof(line + 4, NULL);
        ok("out");
        return;
    }

    // 5) STOP — wymuszenie SAFE (zatrzymanie systemu)
    if (strcmp(line, "stop") == 0){
        app_to_safe(a, SAFE_STOP_CMD, "stop");
        ok("safe");
        return;
    }

    // 6) RUN — powrót do pracy po IDLE (zgodnie z aktualnym mode)
    if (strcmp(line, "run") == 0){
        a->st = (a->mode == MODE_OPEN) ? ST_RUN_OPEN : ST_RUN_CLOSED;
        ok("run");
        return;
    }

    // 7) STAT — telemetria (liczniki)
    if (strcmp(line, "stat") == 0){
        printf("ticks=%u mode_changes=%u sensor_err=%u wd_misses=%u\n",
               (unsigned)a->ticks,
               (unsigned)a->mode_changes,
               (unsigned)a->sensor_err,
               (unsigned)a->wd_misses);
        return;
    }

    // Nieznana komenda
    err("unknown_cmd");
}
