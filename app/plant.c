/*
 * plant.c — prosty symulator obiektu 1-rzędu (host / testy)
 *
 * Model:
 *   y[k+1] = y[k] + alpha * (-y[k] + target)
 *
 * gdzie:
 * - y     : aktualny pomiar (np. temperatura)
 * - alpha : dynamika obiektu (0..1); większe => szybciej reaguje
 * - target: „punkt docelowy” wynikający z ambient + wpływ sterowania
 *
 * Uwaga:
 * - u_percent przyjmujemy jako sterowanie w [%] => 0..100
 * - gain opisuje jak silnie sterowanie wpływa na wartość mierzona
 * - ambient to tło (np. temperatura otoczenia), do której obiekt dąży bez sterowania
 */

#include "plant.h"

// Jeden krok symulacji obiektu; zwraca nowe y
float plant_step(plant_t* p, float u_percent){
    // gain: wpływ sterowania na „target”
    const float gain = 0.30f;

    // Sterowanie w skali 0..1
    const float u = (u_percent / 100.0f);

    // target to „ambient + wpływ sterowania”
    const float target = p->amb + gain * u;

    // Dyskretny model 1-rzędu
    p->y = p->y + p->alpha * (-p->y + target);

    return p->y;
}

void plant_init(plant_t* p, float y0, float alpha, float ambient){
    p->y = y0;
    p->alpha = alpha;
    p->amb = ambient;
}

/*
 * plant_force_y — wstrzyknięcie pomiaru (fault injection).
 * Używane w testach akceptacyjnych do wymuszenia wejścia w SAFE (OOB).
 */
void plant_force_y(plant_t* p, float y){
    p->y = y;
}
