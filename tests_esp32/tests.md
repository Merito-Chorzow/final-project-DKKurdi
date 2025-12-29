# Testy projektu – system alarmu ESP32

## Testy funkcjonalne

### 1. Przejście do RUN
- komenda: `run`
- oczekiwane: ST=RUN, alarm OFF

### 2. Alarm temperaturowy
- ustawienie: `sp 24`
- temperatura > 24°C
- oczekiwane: ALARM=ON, buzzer pika, LED ON

### 3. Histereza
- temperatura spada poniżej `sp − hyst`
- oczekiwane: ALARM=OFF, buzzer OFF

---

## Testy awaryjne

### 4. Błąd czujnika
- odłączenie DHT11
- oczekiwane: SAFE, buzzer OFF

### 5. Zakres temperatury
- temperatura poza [-20, 80]
- oczekiwane: SAFE

### 6. Watchdog
- symulacja braku ticków
- oczekiwane: SAFE

---

## Testy interfejsu
- `stat` – poprawne dane
- `idle` – alarm wyłączony
- `safe` – wymuszone SAFE

---

## Wynik
Wszystkie testy zakończone sukcesem.
System zachowuje się deterministycznie i bezpiecznie.
