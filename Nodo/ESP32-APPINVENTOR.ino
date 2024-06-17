#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <HTTPClient.h>
#include "DHT.h"
#include "WiFi.h"
#include "datos_wifi.h"
#include "time.h"
#include <ESP32Firebase.h>

const char* ssid = SECRET_SSID;
const char* password = SECRET_PSW;
const char* serverName = SECRET_URL;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

#define page 1

#define I2C_SDA 17 
#define I2C_SCL 16

#define DHTPIN 25
#define DHTTYPE DHT11

Adafruit_BMP280 bmp;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);

const int refreshInterval = 60000;
unsigned long lastRefreshTime = 0;

float temperature = 0;
float pressure = 0;
float humidity = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_SDA, I2C_SCL);
  WiFi.begin(ssid, password);
  dht.begin();

  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  while (!Serial)
      delay(10);

  if (!initializeBMP()) {
      Serial.println("BMP280 not found.");
      while (1);
  }

  if (!initializeOLED()) {
      Serial.println(F("SSD1306 allocation failed on address: 0x3C."));
      while (1);
  }

  // Configuración de NTP para obtener el tiempo
  configTime(0, 0, "pool.ntp.org");
  Serial.println("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  Serial.println("Waiting for first measurement... (5 sec)");
  displayInitialScreen();
  delay(5000);
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastRefreshTime >= refreshInterval) {
    display.clearDisplay();
    switch(page){
      case 1:
          readAndDisplayBMPSensorData();
          readAndDisplayDHTSensorData();
          String json = datosJSON(temperature, pressure, humidity);
          String url = etiquetaFireBase();
          enviarJSON(json, url);
          lastRefreshTime = currentTime;
          break;
    }
  }
  display.display();
  delay(50);
}


bool initializeBMP() {
    return bmp.begin(0x76);
}

bool initializeOLED() {
    return display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
}

void displayInitialScreen() {
    display.display();
    delay(2000);
    display.clearDisplay();
}

void readAndDisplayDHTSensorData() {
  humidity = dht.readHumidity();

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  display.setCursor(0, 45);
  display.print("H: ");
  display.print(humidity, 1);
  display.print(" %");
}

void readAndDisplayBMPSensorData() {
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure() / 100.0F;

    Serial.println("");
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.println(" ºC");
    Serial.print("Pres: ");
    Serial.print(pressure);
    Serial.println(" hPa");

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("---------------------");
    display.setCursor(0, 15);
    display.print("T: ");
    display.print(temperature, 1);
    display.print("C");
    display.setCursor(0, 30);
    display.print("Pres: ");
    display.print(pressure, 1);
    display.print(" hPa");
    display.setCursor(0, 60);
    display.print("---------------------");
}

String datosJSON(float temperatura, float presion, float humedad) {
  if (temperatura == NAN) temperatura = 0;
  if (presion == NAN) presion = 0;
  if (humedad == NAN) humedad = 0;

  String json = "{";
  json += "\"Temperatura\":"; json += temperatura; json += ",";
  json += "\"Presion\":"; json += presion; json += ",";
  json += "\"Humedad\":"; json += humedad;
  json += "}";
  return json;
}

void enviarJSON(String jsonData, String url) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  //int httpResponseCode = http.POST(jsonData);
  int httpResponseCode = http.PUT(jsonData);

  Serial.println(jsonData);
  Serial.println(url);

  if (httpResponseCode>0) {
    String response = http.getString();
    Serial.print("httpResponse: ");
    Serial.println(httpResponseCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("Error en la solicitud: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

String etiquetaFireBase(){
  // Obtención de la hora actual
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // Formato de fecha y hora como etiqueta, por ejemplo: "2024-03-22T12:00:00"
    char fecha[40];
    char hora[40];
    strftime(fecha, sizeof(fecha), "%F", &timeinfo);
    strftime(hora, sizeof(hora), "/%H:%M", &timeinfo);

    // Construye la URL completa con la etiqueta de timestamp
    String urlWithTimestamp = String(serverName) + String(fecha) + String(hora) + ".json";
    return urlWithTimestamp;
}