#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <EEPROM.h>
#include "time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ESPmDNS.h>

// ---------------------- CONFIGURACION WIFI Y NTP ----------------------
const char* ssid = "Telecentro-b114";
const char* password = "MKKKNLAVTG4M";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800;  // Argentina GMT-3
const int daylightOffset_sec = 0;

// ---------------------- PINES ----------------------
#define MOTOR_PWM 25
#define MOTOR_AIN1 26
#define MOTOR_AIN2 27
#define LED_VERDE 18
#define LED_ROJO 17
#define BOTON_PIN 19

#define DT 13
#define SCK 12

// ---------------------- EEPROM ----------------------
#define EEPROM_SIZE 1024
#define CONFIG_EEPROM_ADDR 0

// ---------------------- HORARIOS ----------------------
#define MAX_HORARIOS 10
struct Horario {
  int hora;
  int minuto;
  bool activo;
};

// ---------------------- CONFIG (guardado en EEPROM) ----------------------
struct Config {
  float porcion;                       // gramos objetivo
  int numHorarios;
  Horario horarios[MAX_HORARIOS];
  bool modoAutomatico;
  float ultimoPesoDispensado;
};

Config config; // unica fuente de configuracion

// ---------------------- OBJETOS ----------------------
HX711 balanza;
WebServer server(80);

// ---------------------- VARIABLES DE RUNTIME ----------------------
float factorCalibracion = -42061.94;
float pesoPlato = 0.0;
float pesoComida = 0.0;
float minPlato = 0.10;
float maxPlato = 2000.0;

String logBuffer = "";
int ultimoMinuto = -1;

// Maquina de estados
enum EstadoSistema {
  ESTADO_IDLE,
  ESTADO_ESPERA_PLATO,
  ESTADO_DISPENSANDO,
  ESTADO_FIN,
  ESTADO_ERROR
};
EstadoSistema estado = ESTADO_IDLE;

// Temporizadores y control no bloqueante
unsigned long lastMillis = 0;
unsigned long lastButtonMillis = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 200;

// Para control del dispensado (no bloqueante)
bool motorOn = false;
unsigned long dispensadoStartMillis = 0;
unsigned long finDisplayMillis = 0;
const unsigned long FIN_DISPLAY_MS = 1500; // tiempo para mostrar fin

// ---------------------- OLED ----------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
const int OLED_I2C_ADDR = 0x3C; // cambiar si tu pantalla usa otra direccion
const int OLED_SDA = 21;
const int OLED_SCL = 22;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Intervalo para mostrar resumen de configuracion en pantalla (ms)
unsigned long lastConfigDisplayMillis = 0;
const unsigned long CONFIG_DISPLAY_INTERVAL = 10000; // 10s

// ---------------------- FUNCIONES AUXILIARES ----------------------
void logPrint(String msg) {
  Serial.println(msg);
  logBuffer = msg + "<br>" + logBuffer;
  if (logBuffer.length() > 2000) logBuffer.remove(2000);
}

// ---------------------- FUNCIONES MOTOR ----------------------
void motorEncendido() {
  digitalWrite(MOTOR_AIN1, HIGH);
  digitalWrite(MOTOR_AIN2, LOW);
  digitalWrite(MOTOR_PWM, HIGH);
  motorOn = true;
}
void motorApagado() {
  digitalWrite(MOTOR_AIN1, LOW);
  digitalWrite(MOTOR_AIN2, LOW);
  digitalWrite(MOTOR_PWM, LOW);
  motorOn = false;
}

// ---------------------- EEPROM: guardado / carga de config ----------------------
void guardarConfigEEPROM() {
  EEPROM.put(CONFIG_EEPROM_ADDR, config);
  EEPROM.commit();
  logPrint("Config guardada en EEPROM porcion: " + String(config.porcion, 3) + " g");
}
void cargarConfigEEPROM() {
  // valores por defecto
  Config defecto;
  defecto.porcion = 50.0;
  defecto.numHorarios = 0;
  defecto.modoAutomatico = true;
  defecto.ultimoPesoDispensado = 0.0;
  for (int i = 0; i < MAX_HORARIOS; i++) {
    defecto.horarios[i].hora = 0;
    defecto.horarios[i].minuto = 0;
    defecto.horarios[i].activo = false;
  }

  EEPROM.get(CONFIG_EEPROM_ADDR, config);
  // verificar integridad basica
  if (isnan(config.porcion) || config.porcion <= 0 || config.numHorarios < 0 || config.numHorarios > MAX_HORARIOS) {
    config = defecto;
    guardarConfigEEPROM();
    logPrint("EEPROM inicializada con valores por defecto.");
  } else {
    logPrint("Config cargada de EEPROM porcion: " + String(config.porcion, 3) + " g");
  }
}

// ---------------------- SINCRONIZACION / INICIO ALIMENTACION ----------------------
void iniciarAlimentacion(const char* origen) {
  if (estado == ESTADO_DISPENSANDO || estado == ESTADO_ESPERA_PLATO) {
    logPrint(String("Ignorado inicio: ya en proceso origen: ") + origen);
    return;
  }
  logPrint(String("Inicio solicitado desde: ") + origen);
  estado = ESTADO_ESPERA_PLATO;
}

// ---------------------- ALIMENTACION AUTOMATICA ----------------------
void alimentarAutomatica() {
  if (!config.modoAutomatico) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;
  if (m != ultimoMinuto) {
    for (int i = 0; i < config.numHorarios; i++) {
      if (config.horarios[i].activo && config.horarios[i].hora == h && config.horarios[i].minuto == m) {
        logPrint("Hora programada: solicitando inicio automatico porcion: " + String(config.porcion, 3) + " g");
        iniciarAlimentacion("Horario");
      }
    }
    ultimoMinuto = m;
  }
}

// ---------------------- OLED: funciones ----------------------
void oledInit() {
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("ERROR: OLED no detectada");
    return;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
}

void oledShowWifiConnecting() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WiFi: conectando...");
  display.display();
}

void oledShowMenu() {
  struct tm timeinfo;
  char buf[32];
  if (getLocalTime(&timeinfo)) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    snprintf(buf, sizeof(buf), "--:--:--");
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(buf);
  display.setTextSize(1);
  display.setCursor(0, 36);
  display.println("Iniciar la alimentacion");
  display.setCursor(0, 52);
  display.println("Boton / Web / Horario");
  display.display();
}

void oledShowConfigSummary() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Configuracion guardada:");
  char b[40];
  snprintf(b, sizeof(b), "Porcion: %.3f g", config.porcion);
  display.println(b);
  snprintf(b, sizeof(b), "Horarios: %d", config.numHorarios);
  display.println(b);
  display.display();
}

void oledShowPlatoCheck(bool ok, float pesoDetectado) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Verificacion de plato:");
  char b[40];
  snprintf(b, sizeof(b), "Peso: %.2f g", pesoDetectado);
  display.println(b);
  display.setTextSize(2);
  if (ok) {
    display.println("PLATO OK");
  } else {
    display.println("FUERA RANGO");
  }
  display.display();
}

void oledShowProgress(float actual, float objetivo) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  struct tm timeinfo;
  char timebuf[12];
  if (getLocalTime(&timeinfo)) {
    snprintf(timebuf, sizeof(timebuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    snprintf(timebuf, sizeof(timebuf), "--:--");
  }
  display.println(timebuf);
  display.setCursor(0, 14);
  char b[48];
  snprintf(b, sizeof(b), "Servido: %.2f / %.2f g", actual, objetivo);
  display.println(b);
  int barX = 4, barY = 36;
  int barW = SCREEN_WIDTH - 8, barH = 16;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  float pct = 0;
  if (objetivo > 0) pct = actual / objetivo;
  if (pct > 1.0) pct = 1.0;
  int fillW = (int)(barW * pct);
  if (fillW > 0) {
    display.fillRect(barX + 1, barY + 1, fillW - 1, barH - 2, SSD1306_WHITE);
  }
  display.display();
}

void oledShowFinished() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 8);
  display.println("Alimentacion finalizada");
  display.display();
  delay(1000);
  oledShowMenu();
}

// ---------------------- PAGINA PRINCIPAL (sin cambios visuales) ----------------------
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Dispensador</title>";
  html += "<style>body{background-color:black;color:white;font-family:Arial;}h1,h2{text-align:center;}\n\
.container{display:flex;flex-wrap:wrap;justify-content:center;gap:20px;}\n\
.card{border:1px solid white;padding:20px;border-radius:10px;min-width:220px;max-width:250px;}\n\
button,input[type=submit]{background-color:#333;color:white;padding:10px;border:none;border-radius:5px;margin:5px;cursor:pointer;width:100%;}\n\
button:hover,input[type=submit]:hover{background-color:#555;}\n.hidden{display:none;}</style>";
  html += "<script>function toggleForm(){document.getElementById('formHorario').classList.toggle('hidden');}\n\
function toggleLista(){document.getElementById('listaHorarios').classList.toggle('hidden');}</script>";
  html += "</head><body><h1>Dispensador de comida</h1><div class='container'>";

  html += "<div class='card'><h2>Configuracion</h2>";
  html += "<form action='/opcion'><button name='sel' value='1'>Perro chico (0.150 g)</button></form>";
  html += "<form action='/opcion'><button name='sel' value='2'>Perro adulto (0.300 g)</button></form>";
  html += "<form action='/opcion'><button name='sel' value='3'>Gato chico (0.30 g)</button></form>";
  html += "<form action='/opcion'><button name='sel' value='4'>Gato adulto (0.50 g)</button></form></div>";

  html += "<div class='card'><h2>Horarios</h2><button onclick='toggleForm()'>Agregar horario</button>";
  html += "<div id='formHorario' class='hidden'><form action='/agregarHorario'><input name='hora' placeholder='HH'><input name='minuto' placeholder='MM'><input type='submit' value='Agregar'></form></div>";
  html += "<button onclick='toggleLista()'>Lista de horarios</button><div id='listaHorarios' class='hidden'>";
  if (config.numHorarios == 0) {
    html += "<p>No hay horarios configurados.</p>";
  } else {
    for (int i = 0; i < config.numHorarios; i++) {
      html += String(i) + ": " + String(config.horarios[i].hora) + ":" + (config.horarios[i].minuto < 10 ? "0" : "") + String(config.horarios[i].minuto) + " <a href='/eliminarHorario?idx=" + String(i) + "' onclick=\"return confirm('Eliminar?')\">X</a><br>";
    }
  }
  html += "</div></div>";

  html += "<div class='card'><h2>Accion manual</h2><form action='/dispensar'><input type='submit' value='Dispensar comida'></form></div>";

  html += "<div class='card'><h2>Logs</h2><div style='height:200px;overflow-y:scroll;border:1px solid gray;padding:5px;background:#111;'>" + logBuffer + "</div></div>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// ---------------------- HANDLERS WEB ----------------------
void handleOpcion() {
  if (server.hasArg("sel")) {
    int sel = server.arg("sel").toInt();
    switch (sel) {
      case 1: config.porcion = 0.150; break;
      case 2: config.porcion = 0.300; break;
      case 3: config.porcion = 0.30; break;
      case 4: config.porcion = 0.50; break;
    }
    guardarConfigEEPROM();
    logPrint("Configuracion seleccionada desde web: " + String(config.porcion, 3) + " g");
  }
  server.sendHeader("Location", "/");
  server.send(303);
}
void handleAgregarHorario() {
  if (config.numHorarios < MAX_HORARIOS && server.hasArg("hora") && server.hasArg("minuto")) {
    int h = server.arg("hora").toInt();
    int m = server.arg("minuto").toInt();
    config.horarios[config.numHorarios].hora = h;
    config.horarios[config.numHorarios].minuto = m;
    config.horarios[config.numHorarios].activo = true;
    config.numHorarios++;
    guardarConfigEEPROM();
    logPrint("Horario agregado: " + String(h) + ":" + String(m));
  }
  server.sendHeader("Location", "/");
  server.send(303);
}
void handleEliminarHorario() {
  if (server.hasArg("idx")) {
    int idx = server.arg("idx").toInt();
    if (idx >= 0 && idx < config.numHorarios) {
      for (int i = idx; i < config.numHorarios - 1; i++) {
        config.horarios[i] = config.horarios[i + 1];
      }
      config.numHorarios--;
      guardarConfigEEPROM();
      logPrint("Horario eliminado #" + String(idx));
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}
void handleDispensar() {
  iniciarAlimentacion("Web");
  logPrint("Dispensar solicitado desde la web.");
  server.sendHeader("Location", "/");
  server.send(303);
}

// ---------------------- SETUP ----------------------
void setup() {
  Serial.begin(115200);
  delay(50);

  // OLED init
  oledInit();
  oledShowWifiConnecting();

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  unsigned long startConnect = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startConnect < 8000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi no conectado en tiempo limite. Seguira intentando en background.");
  }

  if (MDNS.begin("dispensador")) {
    Serial.println("MDNS iniciado: dispensador.local");
  } else {
    Serial.println("Error iniciando mDNS");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  cargarConfigEEPROM();

  // Balanza
  balanza.begin(DT, SCK);
  balanza.set_scale(factorCalibracion);
  balanza.tare();

  // Pines
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BOTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_AIN1, OUTPUT);
  pinMode(MOTOR_AIN2, OUTPUT);
  pinMode(MOTOR_PWM, OUTPUT);
  motorApagado();

  // Server
  server.on("/", handleRoot);
  server.on("/opcion", handleOpcion);
  server.on("/agregarHorario", handleAgregarHorario);
  server.on("/eliminarHorario", handleEliminarHorario);
  server.on("/dispensar", handleDispensar);
  server.begin();

  logPrint("Servidor web iniciado en: " + WiFi.localIP().toString());
  logPrint("Sistema listo estado IDLE");

  // Menu inicial en OLED
  oledShowMenu();
}

// ---------------------- LOOP (no bloqueante) ----------------------
void loop() {
  unsigned long now = millis();
  server.handleClient();

  // Alimentacion automatica
  alimentarAutomatica();

  // Lectura continua de la balanza con suavizado exponencial
  static float pesoFiltrado = 0.0;
  float lectura = balanza.get_units(5);
  if (isnan(lectura)) lectura = 0.0;
  const float alpha = 0.2;
  pesoFiltrado = (alpha * lectura) + ((1 - alpha) * pesoFiltrado);

  // Boton fisico: debounce no bloqueante
  if (digitalRead(BOTON_PIN) == LOW) {
    if (now - lastButtonMillis > BUTTON_DEBOUNCE_MS) {
      lastButtonMillis = now;
      iniciarAlimentacion("Boton fisico");
      logPrint("Boton fisico inicio solicitado");
    }
  }

  // Mostrar resumen de configuracion periodicamente en OLED
  if (now - lastConfigDisplayMillis > CONFIG_DISPLAY_INTERVAL) {
    lastConfigDisplayMillis = now;
    oledShowConfigSummary();
    delay(800); // breve retardo para que el usuario vea la pantalla
    oledShowMenu();
  }

  // Maquina de estados
  switch (estado) {
    case ESTADO_IDLE:
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, LOW);
      // nada
      break;

    case ESTADO_ESPERA_PLATO:
      // lectura estable del plato
      pesoPlato = balanza.get_units(10);
      if (pesoPlato < 0) pesoPlato = 0;

      if (pesoPlato >= minPlato && pesoPlato <= maxPlato) {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_ROJO, LOW);
        estado = ESTADO_DISPENSANDO;
        pesoComida = 0.0;
        motorEncendido();
        dispensadoStartMillis = now;
        logPrint("Plato detectado correcto peso: " + String(pesoPlato, 2) + " g");
        oledShowPlatoCheck(true, pesoPlato);
        delay(700);
      } else {
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_ROJO, HIGH);
        logPrint("Plato no detectado o fuera de rango peso: " + String(pesoPlato, 2) + " g");
        estado = ESTADO_ERROR;
        finDisplayMillis = now;
        oledShowPlatoCheck(false, pesoPlato);
        delay(1000);
        oledShowMenu();
      }
      break;

    case ESTADO_DISPENSANDO:
      {
        float pesoTotal = balanza.get_units(5);
        if (pesoTotal < 0) pesoTotal = 0;
        pesoComida = pesoTotal - pesoPlato;
        if (pesoComida < 0) pesoComida = 0;

        static unsigned long lastLogPeso = 0;
        if (now - lastLogPeso > 800) {
          lastLogPeso = now;
          logPrint("Comida servida: " + String(pesoComida, 2) + " g");
        }

        // actualizar OLED con barra de progreso
        oledShowProgress(pesoComida, config.porcion);

        // seguridad: plato fuera de rango
        if (pesoTotal < minPlato || pesoTotal > maxPlato) {
          motorApagado();
          logPrint("Plato fuera de rango durante dispensado. Abortando.");
          estado = ESTADO_ERROR;
          finDisplayMillis = now;
          oledShowPlatoCheck(false, pesoTotal);
          delay(1000);
          oledShowMenu();
          break;
        }

        if (pesoComida >= config.porcion) {
          motorApagado();
          logPrint("Porcion completada correctamente peso: " + String(pesoComida, 2) + " g");
          config.ultimoPesoDispensado = pesoComida;
          guardarConfigEEPROM();
          estado = ESTADO_FIN;
          finDisplayMillis = now;
          oledShowFinished();
          // limpieza posterior realizada en ESTADO_FIN
        }
      }
      break;

    case ESTADO_FIN:
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_ROJO, LOW);
      if (now - finDisplayMillis > FIN_DISPLAY_MS) {
        balanza.tare();
        pesoPlato = 0;
        pesoComida = 0;
        estado = ESTADO_IDLE;
        logPrint("Vuelto a estado IDLE");
        oledShowMenu();
      }
      break;

    case ESTADO_ERROR:
      if (now - finDisplayMillis > FIN_DISPLAY_MS) {
        balanza.tare();
        motorApagado();
        pesoPlato = 0;
        pesoComida = 0;
        estado = ESTADO_IDLE;
        logPrint("Error reseteado, estado IDLE");
        oledShowMenu();
      }
      break;
  }

  // retardo minimo no bloqueante
  delay(10);
}
