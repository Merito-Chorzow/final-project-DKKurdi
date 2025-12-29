/*
 * control.c — regulator PI (float) z anti-windup (clamping) + telemetria
 *
 * Rola modułu:
 * - Dostarcza prosty regulator PI: u = Kp*e + I
 * - Chroni całkę przed „windup” przez ograniczenie i_acc (clamping)
 * - Liczy podstawową telemetrię: licznik saturacji oraz max overshoot
 *
 * Uwaga:
 * - Tick czasowy (Ts) jest „wchłonięty” w Ki (czyli Ki jest już w jednostkach na tick).
 * - Wyjście u jest ograniczane do [u_min, u_max].
 */

#include "control.h"

static float clampf(float x, float lo, float hi){
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void pi_init(pi_t* c, float kp, float ki, float i_limit, float u_min, float u_max){
    // Parametry regulatora
    c->kp = kp;
    c->ki = ki;

    // Stan całki + ograniczenie anti-windup
    c->i_acc = 0.0f;
    c->i_limit = i_limit;

    // Ograniczenia wyjścia (saturacja)
    c->u_min = u_min;
    c->u_max = u_max;

    // Telemetria
    c->u_sat_count = 0;
    c->overshoot_max = 0.0f;
}

void pi_reset(pi_t* c){
    // Reset stanu regulatora (np. przy zmianie trybu OPEN↔CLOSED)
    c->i_acc = 0.0f;

    // Reset telemetrii dla nowego przebiegu testowego
    c->u_sat_count = 0;
    c->overshoot_max = 0.0f;
}

float pi_step(pi_t* c, float setpoint, float meas){
    // Błąd regulacji: e = SP - PV
    const float e = setpoint - meas;

    // Człon P: natychmiastowa reakcja proporcjonalna do błędu
    const float p = c->kp * e;

    // Człon I: akumulacja błędu w czasie.
    // Anti-windup: ograniczamy i_acc, aby układ nie „pompował” całki przy saturacji.
    c->i_acc += c->ki * e;
    c->i_acc = clampf(c->i_acc, -c->i_limit, +c->i_limit);

    // Suma przed saturacją
    const float u_raw = p + c->i_acc;

    // Saturacja wyjścia do zakresu aktuatora
    const float u = clampf(u_raw, c->u_min, c->u_max);

    // Telemetria: ile razy regulator „uderzył” w ograniczenia
    if (u != u_raw) c->u_sat_count++;

    // Telemetria: overshoot (dla dodatniego setpointu).
    // Overshoot liczymy jako (PV - SP) jeśli PV przekroczyło SP.
    if (setpoint > 0.0f) {
        const float os = meas - setpoint;
        if (os > c->overshoot_max) c->overshoot_max = os;
    }

    return u;
}
