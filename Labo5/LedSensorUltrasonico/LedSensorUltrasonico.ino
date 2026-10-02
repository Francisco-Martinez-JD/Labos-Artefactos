#include <WiFi.h>                     
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ---------------------- CONFIGURACIÓN WI-FI ----------------------
#define WLAN_SSID   "ARTEFACTOS"
#define WLAN_PASS   "87654321"

// ---------------------- CONFIGURACIÓN ADAFRUIT IO ----------------------
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    ""   
#define AIO_KEY         ""           //(no la subas a repositorio)

// ---------------------- PINES ----------------------
#define TRIG_PIN  23
#define ECHO_PIN 34
#define PIN_R 16
#define PIN_G 17
#define PIN_B 18

// ---------------------- AJUSTES ----------------------
#define RGB_ANODO_COMUN  false      
#define PWM_FREQ         5000
#define PWM_RES          8         
#define INTERVALO_PUBLICAR_MS 5000  // Adafruit gratis limita los datos por minuto: no bajar de 4-5 s
#define DIST_CERCA_CM  10     
#define DIST_MEDIA_CM  30   

// ---------------------- CLIENTE MQTT Y FEED ----------------------
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Publicar
Adafruit_MQTT_Publish feedDistancia = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/distancia");
Adafruit_MQTT_Subscribe feedBoton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/FeedLedBoton");


// ---------------------- ESTADO ----------------------
float ultimaDistancia = -1;             // última lectura válida (cm)
unsigned long ultimoPublicar = 0;
unsigned long ultimoPing = 0;
bool ledEncendido = false;

// ---------------------- PROTOTIPOS ----------------------
void conectarWiFi();
void conectarMQTT();
float leerDistanciaCm();
float distanciaPromedio(int muestras);
void escribirRGB(uint8_t r, uint8_t g, uint8_t b);
void actualizarLED();

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  ledcAttach(PIN_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_B, PWM_FREQ, PWM_RES);

  escribirRGB(255, 255, 255);   

  conectarWiFi();

  mqtt.subscribe(&feedBoton);
}

// =====================================================================
void loop() {
  conectarMQTT();   // mantiene la conexión con Adafruit (reconecta si se cae)
    
  Adafruit_MQTT_Subscribe *sub;

  while ((sub = mqtt.readSubscription(200))) {

    if (sub == &feedBoton) {

      const char *msg = (char *)feedBoton.lastread;

      ledEncendido = (strcmp(msg, "ON") == 0);

      Serial.print("boton-led: ");
      Serial.println(msg);

      actualizarLED();
    }
  }

  // Leer el ultrasónico y publicar cada cierto tiempo
  if (millis() - ultimoPublicar >= INTERVALO_PUBLICAR_MS) {
    ultimoPublicar = millis();

    float d = distanciaPromedio(5);
    if (d > 0) {
      ultimaDistancia = d;
      Serial.print("Distancia: "); Serial.print(d, 1); Serial.println(" cm");

      if (!feedDistancia.publish(d)) {
        Serial.println("Error al publicar la distancia");
      }
    } else {
      Serial.println("Lectura fuera de rango o sin eco");
    }

    actualizarLED();   // el color depende de la última distancia leída
  }

  // Mantener viva la conexión MQTT
  if (millis() - ultimoPing >= 30000) {
    ultimoPing = millis();
    mqtt.ping();
  }
}

// =====================================================================
//  ULTRASÓNICO HC-SR04
// =====================================================================
float leerDistanciaCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return -1;   // no hubo eco

  float distancia = ((0.0343*duracion)/2);

  // Rango útil del sensor: 2 a 100 cm
  if (distancia < 2 || distancia > 100) return -1;
  return distancia;
}

// Promedia varias lecturas para reducir el ruido
float distanciaPromedio(int muestras) {
  float suma = 0;
  int validas = 0;
  for (int i = 0; i < muestras; i++) {
    float d = leerDistanciaCm();
    if (d > 0) { suma += d; validas++; }
    delay(40);   // espera entre mediciones para que no se mezclen los ecos
  }
  return (validas > 0) ? suma / validas : -1;
}

// =====================================================================
//  LED RGB
// =====================================================================
void escribirRGB(uint8_t r, uint8_t g, uint8_t b) {
  if (RGB_ANODO_COMUN) {          
    r = 255 - r;  g = 255 - g;  b = 255 - b;
  }
  ledcWrite(PIN_R, r);
  ledcWrite(PIN_G, g);
  ledcWrite(PIN_B, b);
}

// El color debe cambiar solo según la última distancia leída.
void actualizarLED() {
  if (ledEncendido == false) {
    escribirRGB(0, 0, 0);   
    return;
  }

  if (ultimaDistancia < DIST_CERCA_CM) {
    escribirRGB(50, 75, 100);   
  } else if ((ultimaDistancia > DIST_CERCA_CM) && (ultimaDistancia < DIST_MEDIA_CM)) {
    escribirRGB(150, 125, 100);   
  } else {
    escribirRGB(255, 0, 255);   

  }
}

// =====================================================================
//  CONEXIONES
// =====================================================================
void conectarWiFi() {
  Serial.print("Conectando a "); Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi conectado. IP: "); Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  if (mqtt.connected()) return;

  Serial.print("Conectando a Adafruit IO... ");
  int8_t ret;
  uint8_t intentos = 3;
  while ((ret = mqtt.connect()) != 0) {         
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Reintentando en 5 segundos...");
    mqtt.disconnect();
    delay(5000);
    if (--intentos == 0) {
      Serial.println("No se pudo conectar. Reiniciando la ESP32...");
      ESP.restart();
    }
  }
  Serial.println("¡Conectado a Adafruit IO!");
}