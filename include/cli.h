#pragma once
/*
 * cli.h — interfejs modułu CLI (protokół tekstowy)
 *
 * CLI jest lekkim interfejsem do sterowania i diagnostyki:
 * - umożliwia testy akceptacyjne bez sprzętu,
 * - pozwala sterować FSM oraz parametrami regulatora,
 * - ułatwia logowanie/telemetrię poprzez stdout/UART.
 */

#include "fsm.h"

// Obsłuż pojedynczą linię komendy (bez '\n')
void cli_handle_line(app_t* a, const char* line);
