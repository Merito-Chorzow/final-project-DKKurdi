# Design projektu – system alarmu temperaturowego ESP32

## Architektura
System zbudowany jest warstwowo:

- **Warstwa sprzętowa**  
  DHT11, OLED, buzzer, LED

- **Warstwa logiki aplikacji**
  - FSM
  - logika alarmu z histerezą
  - watchdog

- **Warstwa interfejsu**
  - CLI po UART (Serial)

---

## Maszyna stanów (FSM)

INIT → IDLE → RUN

↓

SAFE


### Opis stanów
- **INIT** – inicjalizacja peryferiów
- **IDLE** – brak aktywnej regulacji
- **RUN** – aktywny monitoring i alarm
- **SAFE** – stan bezpieczny po błędach

---

## Logika alarmu
Alarm działa z histerezą:

- ALARM ON: `T ≥ setpoint`
- ALARM OFF: `T ≤ setpoint − hyst`

Zapobiega to częstemu przełączaniu stanu alarmu (flapping).

---

## Watchdog
Watchdog aplikacyjny monitoruje poprawne wywoływanie funkcji `app_tick()`:

- brak ticków przez > 500 ms → SAFE
- watchdog resetowany przy każdym poprawnym ticku

---

## Aktuatory
- **Buzzer (PWM)** – sygnalizacja alarmu (pulsacyjna)
- **LED** – sygnalizacja binarna alarmu
- **OLED** – wizualizacja stanu systemu

---

## Bezpieczeństwo
System przechodzi do SAFE w przypadku:
- błędu odczytu sensora
- przekroczenia zakresu temperatury
- timeoutu watchdog
- ręcznej komendy `safe`
