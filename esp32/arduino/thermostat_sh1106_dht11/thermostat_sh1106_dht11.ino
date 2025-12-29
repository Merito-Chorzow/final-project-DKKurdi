/*
 * Projekt: System alarmu temperaturowego na ESP32 (DHT11 + OLED SH1106 + buzzer)
 * Autor: [Daniel Kurdek]
 *
 * Cel:
 * - cyklicznie odczytywać temperaturę i wilgotność z DHT11,
 * - wyświetlać bieżące dane na OLED (I2C),
 * - uruchamiać alarm z histerezą (bez „flappingu”),
 * - sygnalizować alarm diodą LED i buzzerem PWM,
 * - posiadać FSM (INIT/IDLE/RUN/SAFE) + watchdog aplikacyjny,
 * - umożliwić konfigurację przez CLI po UART (Serial).
 *
 * Założenia czasowe:
 * - tick logiki: 10 ms (TICK_MS) — deterministyczny krok systemu
 * - odczyt DHT: co 2 s (DHT_MS) — zgodnie z ograniczeniami DHT11
 * - odświeżanie OLED: co 200 ms (OLED_MS) — płynny UI, bez migotania
 *
 * Uwaga:
 * - W logice nie używam delay() (poza krótkim startem w setup),
 *   aby system był nieblokujący i przewidywalny.
 */

#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ====================================================
// 1) PINOUT / HW CONFIG
// ====================================================

// DHT11: DATA do GPIO4
#define DHTPIN   4
#define DHTTYPE  DHT11

// OLED SH1106: I2C
#define I2C_SDA  21
#define I2C_SCL  22

// LED testowa (opcjonalna): sygnalizacja alarmu
#define ACT_PIN  2

// Buzzer PWM (ESP32 Arduino core 3.x):
// Uwaga: w core 3.x używam ledcAttach(pin, freq, res) oraz ledcWrite(pin, duty).
static const int BUZZ_PIN      = 23;
static const int BUZZ_PWM_FREQ = 2000; // 2 kHz: typowo słyszalny „pisk”
static const int BUZZ_PWM_RES  = 8;    // 8-bit PWM => duty 0..255
static const int BUZZ_DUTY_ON  = 128;  // ~50% wypełnienia

// ====================================================
// 2) OBIEKTY PERYPHERIÓW
// ====================================================

Adafruit_SH1106G display(128, 64, &Wire);
DHT dht(DHTPIN, DHTTYPE);

// ====================================================
// 3) FSM + REASONS (stan aplikacji i powody SAFE)
// ====================================================

// Stany systemu: świadomie proste (MVP), łatwe do rozbudowy.
enum AppState : uint8_t {
  ST_INIT = 0,   // inicjalizacja (krótki stan przejściowy)
  ST_IDLE,       // system gotowy, alarm nieaktywny
  ST_RUN,        // aktywny monitoring i alarm
  ST_SAFE        // stan bezpieczny po błędzie (latched)
};

enum SafeReason : uint8_t {
  SAFE_NONE = 0,
  SAFE_SENSOR_FAIL, // np. NaN z DHT
  SAFE_SENSOR_OOB,  // pomiar poza zakresem bezpieczeństwa
  SAFE_WATCHDOG     // brak ticków logiki zbyt długo
};

static AppState   g_state = ST_INIT;
static SafeReason g_safe_reason = SAFE_NONE;

// ====================================================
// 4) DANE PROCESU + PARAMETRY (konfiguracja użytkownika)
// ====================================================

// Bieżące pomiary:
static float g_tempC = 0.0f;
static float g_hum   = 0.0f;

// Konfiguracja alarmu:
static float g_setpoint = 28.0f; // próg temperatury (°C)
static float g_hyst     = 1.0f;  // histereza (°C)

// Alarm wewnętrzny:
// UWAGA: nazwa "alarm" koliduje z funkcją systemową -> używamy alarm_on
static bool     g_alarm_on  = false;

// Telemetria:
static uint32_t g_ticks     = 0;
static uint32_t g_wd_misses = 0;

// ====================================================
// 5) HARMONOGRAM (millis) + LIMITY BEZPIECZEŃSTWA
// ====================================================

static const uint32_t TICK_MS = 10;    // krok logiki
static const uint32_t DHT_MS  = 2000;  // DHT11: nie czytać częściej niż ~1–2 s
static const uint32_t OLED_MS = 200;   // UI odświeżamy częściej niż DHT

static uint32_t g_last_tick_ms = 0;
static uint32_t g_last_dht_ms  = 0;
static uint32_t g_last_oled_ms = 0;

// Zakres bezpieczeństwa dla temperatury:
static const float    T_MIN = -20.0f;
static const float    T_MAX =  80.0f;

// Watchdog: ile ticków bez „kick” dopuszczamy.
// 50 * 10 ms = 500 ms
static const uint32_t WD_MAX_MISSES = 50;

// ====================================================
// 6) BUZZER (sterowanie nieblokujące)
// ====================================================

static void buzzer_on()  { ledcWrite(BUZZ_PIN, BUZZ_DUTY_ON); }
static void buzzer_off() { ledcWrite(BUZZ_PIN, 0); }

/*
 * buzzer_beep(enable)
 * - Jeśli enable==true: przełącza stan buzzer ON/OFF co ~300 ms (pulsowanie).
 * - Jeśli enable==false: gwarantuje wyłączenie buzzera.
 *
 * Brak delay() => nieblokujące, dobre dla embedded (system pozostaje responsywny).
 */
static void buzzer_beep(bool enable){
  static uint32_t last_toggle = 0;
  static bool state = false;

  if (!enable){
    buzzer_off();
    state = false;
    return;
  }

  const uint32_t now = millis();
  if (now - last_toggle >= 300){
    last_toggle = now;
    state = !state;
    if (state) buzzer_on();
    else       buzzer_off();
  }
}

// ====================================================
// 7) SAFE (przejście do stanu bezpiecznego)
// ====================================================

static const char* reason_str(SafeReason r){
  switch(r){
    case SAFE_SENSOR_FAIL: return "sensor_fail";
    case SAFE_SENSOR_OOB:  return "sensor_oob";
    case SAFE_WATCHDOG:    return "watchdog";
    default:               return "none";
  }
}

/*
 * to_safe(reason, msg)
 * - Ustawia stan SAFE i „wyłącza” aktywne wyjścia.
 * - SAFE jest latched (nie wychodzimy automatycznie) — decyzja bezpieczeństwa.
 */
static void to_safe(SafeReason r, const char* msg){
  g_state = ST_SAFE;
  g_safe_reason = r;
  g_alarm_on = false;

  digitalWrite(ACT_PIN, LOW);
  buzzer_off();

  Serial.print("[SAFE] reason=");
  Serial.print(reason_str(r));
  Serial.print(" msg=");
  Serial.print(msg ? msg : "-");
  Serial.print(" ticks=");
  Serial.println(g_ticks);
}

// ====================================================
// 8) WATCHDOG aplikacyjny
// ====================================================

static void watchdog_kick(){ g_wd_misses = 0; }

/*
 * watchdog_tick()
 * - Wołany co TICK_MS.
 * - Jeżeli logika aplikacji przestanie „karmić” watchdog, wejdziemy w SAFE.
 */
static void watchdog_tick(){
  if (g_state == ST_SAFE) return;
  if (++g_wd_misses > WD_MAX_MISSES){
    to_safe(SAFE_WATCHDOG, "no app_tick()");
  }
}

// ====================================================
// 9) LOGIKA ALARMU (histereza)
// ====================================================

/*
 * update_alarm_logic()
 * - Aktywny tylko w ST_RUN.
 * - ON:  T >= setpoint
 * - OFF: T <= setpoint - hyst
 * Zapobiega flappingowi przy temperaturze blisko progu.
 */
static void update_alarm_logic(){
  if (g_state != ST_RUN) { g_alarm_on = false; return; }

  if (!g_alarm_on && g_tempC >= g_setpoint) g_alarm_on = true;
  else if (g_alarm_on && g_tempC <= (g_setpoint - g_hyst)) g_alarm_on = false;
}

// ====================================================
// 10) SENSOR: odczyt DHT (co 2 s)
// ====================================================

static void read_dht(){
  const float nt = dht.readTemperature();
  const float nh = dht.readHumidity();

  // DHT potrafi zwrócić NaN — traktujemy jako błąd sensora
  if (isnan(nt) || isnan(nh)){
    to_safe(SAFE_SENSOR_FAIL, "DHT read failed");
    return;
  }

  // Walidacja zakresu: ochrona przed błędami / zwarciem / złym podłączeniem
  if (nt < T_MIN || nt > T_MAX){
    to_safe(SAFE_SENSOR_OOB, "T out of range");
    return;
  }

  g_tempC = nt;
  g_hum   = nh;
}

// ====================================================
// 11) OLED: prezentacja danych (UI)
// ====================================================

static void oled_draw(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);

  display.print("ST:");
  display.print((int)g_state);
  display.print("  AL:");
  display.println(g_alarm_on ? "ON" : "OFF");

  display.print("T: ");
  display.print(g_tempC, 1);
  display.println(" C");

  display.print("H: ");
  display.print(g_hum, 0);
  display.println(" %");

  display.print("SP:");
  display.print(g_setpoint, 1);
  display.print(" H:");
  display.println(g_hyst, 1);

  if (g_state == ST_SAFE){
    display.print("SAFE: ");
    display.println(reason_str(g_safe_reason));
  }

  display.display();
}

// ====================================================
// 12) APP TICK: deterministyczny krok logiki systemu
// ====================================================

/*
 * app_tick()
 * - deterministyczny „krok” systemu (co 10 ms)
 * - utrzymuje FSM
 * - karmi watchdog
 * - steruje wyjściami (LED, buzzer)
 */
static void app_tick(){
  g_ticks++;
  watchdog_kick();

  switch(g_state){
    case ST_INIT:
      g_state = ST_IDLE;
      break;

    case ST_IDLE:
      g_alarm_on = false;
      break;

    case ST_RUN:
      update_alarm_logic();
      break;

    case ST_SAFE:
      g_alarm_on = false;
      break;
  }

  // Wyjścia:
  digitalWrite(ACT_PIN, g_alarm_on ? HIGH : LOW);
  buzzer_beep(g_alarm_on);
}

// ====================================================
// 13) CLI (UART Serial): prosta konfiguracja
// ====================================================

/*
 * Dostępne komendy:
 * - help
 * - run / idle / safe
 * - sp <val>
 * - hyst <val>
 * - stat
 */
static void handle_line(String s){
  s.trim();
  s.toLowerCase();

  if (s == "help"){
    Serial.println("cmd: run | idle | safe | sp <val> | hyst <val> | stat");
    return;
  }

  if (s == "run"){
    if (g_state != ST_SAFE) g_state = ST_RUN;
    Serial.println("OK RUN");
    return;
  }

  if (s == "idle"){
    if (g_state != ST_SAFE) g_state = ST_IDLE;
    g_alarm_on = false;
    buzzer_off();
    Serial.println("OK IDLE");
    return;
  }

  if (s == "safe"){
    to_safe(SAFE_NONE, "manual");
    return;
  }

  if (s.startsWith("sp ")){
    g_setpoint = s.substring(3).toFloat();
    Serial.println("OK sp");
    return;
  }

  if (s.startsWith("hyst ")){
    g_hyst = s.substring(5).toFloat();
    Serial.println("OK hyst");
    return;
  }

  if (s == "stat"){
    Serial.print("st=");   Serial.print((int)g_state);
    Serial.print(" T=");   Serial.print(g_tempC);
    Serial.print(" H=");   Serial.print(g_hum);
    Serial.print(" sp=");  Serial.print(g_setpoint);
    Serial.print(" hyst=");Serial.print(g_hyst);
    Serial.print(" alarm=");Serial.println(g_alarm_on ? "ON":"OFF");
    return;
  }

  Serial.println("ERR unknown; type: help");
}

// ====================================================
// 14) Arduino entry points: setup() / loop()
// ====================================================

void setup(){
  Serial.begin(115200);
  delay(300);

  // PWM init buzzera
  ledcAttach(BUZZ_PIN, BUZZ_PWM_FREQ, BUZZ_PWM_RES);
  buzzer_off();

  // LED
  pinMode(ACT_PIN, OUTPUT);
  digitalWrite(ACT_PIN, LOW);

  // I2C + OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(0x3C, true)) {
    Serial.println("OLED init fail");
  }
  display.setRotation(0);

  // DHT
  dht.begin();

  Serial.println("READY. Type 'help'.");
  g_state = ST_INIT;

  // Pierwszy odczyt i startowy ekran
  read_dht();
  oled_draw();
}

void loop(){
  // 1) CLI: odbiór komend po UART
  if (Serial.available()){
    String line = Serial.readStringUntil('\n');
    handle_line(line);
  }

  const uint32_t now = millis();

  // 2) Tick logiki + watchdog (10 ms)
  if (now - g_last_tick_ms >= TICK_MS){
    g_last_tick_ms += TICK_MS;
    watchdog_tick();
    app_tick();
  }

  // 3) Odczyt DHT (2 s)
  if (now - g_last_dht_ms >= DHT_MS){
    g_last_dht_ms = now;
    if (g_state != ST_SAFE) read_dht();
  }

  // 4) Odświeżanie OLED (200 ms)
  if (now - g_last_oled_ms >= OLED_MS){
    g_last_oled_ms = now;
    oled_draw();
  }
}
