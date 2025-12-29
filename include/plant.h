#pragma once
/*
 * plant.h — symulator obiektu 1-rzędu (host/testy)
 *
 * Model:
 *   y[k+1] = y[k] + alpha * (-y[k] + target)
 *   target = amb + gain * u
 *
 * gdzie:
 * - y     : aktualna wartość procesu (np. temperatura)
 * - alpha : dynamika obiektu (0..1); większe => szybsza reakcja
 * - amb   : „otoczenie” (bazowa wartość, do której obiekt dąży bez sterowania)
 *
 * Sterowanie:
 * - u_percent: 0..100 (procent wysterowania), spójne z u w aplikacji.
 */

typedef struct {
    float y;        // wartość procesu (np. temperatura)
    float alpha;    // dynamika obiektu
    float amb;      // wartość tła („otoczenie”)
} plant_t;

void  plant_init(plant_t* p, float y0, float alpha, float ambient);
float plant_step(plant_t* p, float u_percent);  // u: 0..100
void  plant_force_y(plant_t* p, float y);
