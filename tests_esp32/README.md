# Projekt zaliczeniowy – Programowanie niskopoziomowe (ESP32)

## Opis projektu
Projekt przedstawia prosty system monitoringu temperatury oparty o ESP32.
Dane z czujnika DHT11 są przetwarzane w deterministycznej pętli czasowej, a na ich podstawie realizowana jest logika alarmowa z histerezą.
System posiada maszynę stanów (FSM), watchdog aplikacyjny oraz interfejs CLI.

Projekt został uruchomiony i przetestowany na rzeczywistym sprzęcie.

---

## Funkcjonalności
- odczyt temperatury i wilgotności (DHT11)
- wyświetlanie danych na OLED (SH1106, I2C)
- alarm temperaturowy z histerezą
- sygnalizacja alarmu:
  - buzzer PWM
  - dioda LED
- FSM: INIT → IDLE → RUN → SAFE
- watchdog aplikacyjny
- CLI po Serial (UART)

---

## Sprzęt
- ESP32 Dev Module
- DHT11 (GPIO4)
- OLED SH1106 128×64 (I2C)
- Buzzer (GPIO23, PWM)
- LED testowa (GPIO2)

### Połączenia

 DHT11 DATA -> GPIO4 
 OLED SDA -> GPIO21 
 OLED SCL -> GPIO22 
 Buzzer -> GPIO23 
 LED -> GPIO2 

---

## Uruchomienie
1. Zainstaluj Arduino IDE
2. Zainstaluj pakiet **ESP32 (Arduino core 3.x)**
3. Podłącz ESP32 przez USB
4. Wgraj plik `.ino`
5. Otwórz Serial Monitor (115200)

---

## Komendy CLI

help – lista komend

run – przejście do RUN

idle – przejście do IDLE

safe – ręczne wejście w SAFE

sp <val> – ustaw setpoint temperatury

hyst <val> – ustaw histerezę

stat – status systemu


---

## Autor
Daniel Kurdek 131786

Programowanie niskopoziomowe – projekt zaliczeniowy
