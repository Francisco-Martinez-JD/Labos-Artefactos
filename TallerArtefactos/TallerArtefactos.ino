#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================

const char* WIFI_SSID = "ARTEFACTOS";
const char* WIFI_PASSWORD = "s^3sQ#9teskVgnb";

// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================

const char* MQTT_SERVER = "10.110.0.4";
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React.
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================

//Francisco Javier Martinez Donado 00073424

#define DHT_PIN 4        
#define DHT_TYPE DHT11
#define LDR_PIN 32       

DHT dht(DHT_PIN, DHT_TYPE);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Enviar una lectura cada 2 segundos.
const unsigned long INTERVALO_ENVIO = 2000;

unsigned long ultimoEnvio = 0;

void conectarWiFi() {

  Serial.print("Conectando a WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");

  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Conectando a Mosquitto...");

    // Crear un Client ID diferente para este ESP32.
    String clientId = "ESP32-";
    clientId += String(
      (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF),
      HEX
    );

    if (mqttClient.connect(clientId.c_str())) {

      Serial.println(" conectado");

    } else {

      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");

      delay(2000);
    }
  }
}

// ======================================================
// LEER Y PUBLICAR SENSORES
// ======================================================

void publicarSensores() {

  // Obtener temperatura 
  float temperatura = dht.readTemperature();
 //humedad del DHT11
  float humedad = dht.readHumidity();

  // 3. Obtener la lectura de la LDR .
  int ldrAdc = analogRead(LDR_PIN);
  int luz = map(ldrAdc, 0, 4095, 0, 100);

  // 4. Preparar los valores como texto para enviarlos por MQTT.
  char bufTemp[50];
  char bufHum[50];
  char bufLuz[50];
  snprintf(bufTemp, sizeof(bufTemp), "%.2f", temperatura);
  snprintf(bufHum, sizeof(bufHum), "%.2f", humedad);
  snprintf(bufLuz, sizeof(bufLuz), "%d", luz);

  // 5. Publicar cada dato en su topic (true = mensaje retained).
  if (!isnan(temperatura) && !isnan(humedad)) {
    mqttClient.publish(TOPIC_TEMPERATURA, bufTemp, true);
    mqttClient.publish(TOPIC_HUMEDAD, bufHum, true);
  } else {
    Serial.println("Error leyendo el DHT11");
  }
  mqttClient.publish(TOPIC_LUZ, bufLuz, true);

  // 6. Mostrar las mediciones en el Monitor Serial.
  Serial.print("Temperatura: "); Serial.print(bufTemp); Serial.println(" °C");
  Serial.print("Humedad: ");     Serial.print(bufHum);  Serial.println(" %");
  Serial.print("LDR ADC: ");     Serial.print(ldrAdc);
  Serial.print(" | Luz: ");      Serial.print(luz);     Serial.println(" %");
  Serial.println("------------------------------");
}

void setup() {

  Serial.begin(115200);

  // Inicializar los sensores.
  // GPIO 32 es entrada analógica: analogRead no necesita pinMode.
  dht.begin();
  delay(2000);  // Tiempo para que el DHT11 se estabilice.

  conectarWiFi();

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );
}

void loop() {

  // Si se pierde WiFi, volver a conectarse.
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Si se pierde MQTT, volver a conectarse.
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  // Publicar sensores cada 2 segundos.
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {

    ultimoEnvio = millis();

    publicarSensores();
  }
}