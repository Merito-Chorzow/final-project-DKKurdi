#pragma once
/*
 * control.h — regulator PI (float) z anti-windup i telemetrią
 *
 * PI:
 *   e  = setpoint - meas
 *   P  = Kp * e
 *   I  = I + Ki * e         (Ki uwzględnia Ts w dyskretnej implementacji)
 *   I  = clamp(I, ±i_limit) (anti-windup przez clamping)
 *   u  = clamp(P + I, [u_min, u_max])
 *
 * Telemetria:
 * - u_sat_count  : ile razy wystąpiła saturacja (u != u_raw)
 * - overshoot_max: maksymalne przekroczenie (meas - setpoint), gdy meas > setpoint
 */

#include <stdint.h>

typedef struct {
    // Parametry regulatora
    float kp;
    float ki;

    // Stan całki + ograniczenie anti-windup
    float i_acc;
    float i_limit;

    // Ograniczenia wyjścia
    float u_min;
    float u_max;

    // Telemetria
    uint32_t u_sat_count;
    float    overshoot_max;
} pi_t;

void  pi_init(pi_t* c, float kp, float ki, float i_limit, float u_min, float u_max);
float pi_step(pi_t* c, float setpoint, float meas);
void  pi_reset(pi_t* c);
