#include <WiFi.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// WiFi & MQTT cấu hình
const char* ssid = "Tầng 2";
const char* password = "lacaigivay";
const char* mqtt_server = "f8e9523a8a3b48ea957832a96e91caa0.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "e-smart";
const char* mqtt_password = "Abc@1234";

// MQTT client qua TLS
WiFiClientSecure espClient;
PubSubClient client(espClient);

// GPIO output cho LED
const int out[2] = {25, 33};
bool updateState = 0;

// Khai báo BMP280 dùng SPI (chân tùy chỉnh)
#define BMP_SCK  14
#define BMP_MISO 12
#define BMP_MOSI 13
#define BMP_CS   27
Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO, BMP_SCK);

void setup_wifi() {
  Serial.print("Kết nối WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi đã kết nối!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Kết nối MQTT...");
    String clientID = "ESP32Client-";
    clientID += String(random(0xffff), HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Đã kết nối MQTT");
      client.subscribe("esp32/client");
    } else {
      Serial.print("Thất bại, mã lỗi = ");
      Serial.print(client.state());
      Serial.println(". Thử lại sau 5s");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("Tin nhắn MQTT nhận được [" + String(topic) + "]: " + message);

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("Lỗi parse JSON: " + String(error.c_str()));
    return;
  }

  if (doc.containsKey("out1")) {
    bool p = doc["out1"];
    digitalWrite(out[0], p);
    Serial.println("out1: " + String(p));
  }
  if (doc.containsKey("out2")) {
    bool p = doc["out2"];
    digitalWrite(out[1], p);
    Serial.println("out2: " + String(p));
  }
  updateState = true;
}

void publishMessage(const char* topic, String payload, boolean retained) {
  if (client.publish(topic, payload.c_str(), retained)) {
    Serial.println("Đã gửi [" + String(topic) + "]: " + payload);
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  espClient.setInsecure();  // Bỏ qua SSL verify
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Cấu hình output LED
  for (int i = 0; i < 2; i++) {
    pinMode(out[i], OUTPUT);
    digitalWrite(out[i], LOW);
  }

  // Khởi tạo BMP280 SPI
  if (!bmp.begin()) {
    Serial.println("Không tìm thấy cảm biến BMP280 qua SPI!");
    while (1);
  }
  Serial.println("Đã kết nối BMP280 (SPI).");
}

unsigned long lastSensorTime = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Gửi dữ liệu BMP280 mỗi 1 giây
  if (millis() - lastSensorTime > 1000) {
    float temperature = bmp.readTemperature();
    float pressure = bmp.readPressure() / 100.0F;

    if (!isnan(temperature) && !isnan(pressure)) {
      DynamicJsonDocument doc(256);
      doc["temperature"] = temperature;
      doc["pressure"] = pressure;
      char mqtt_message[128];
      serializeJson(doc, mqtt_message);
      publishMessage("esp32/bmp280", mqtt_message, true);
    }
    lastSensorTime = millis();
  }

  // Gửi lại trạng thái LED nếu có thay đổi
  if (updateState) {
    DynamicJsonDocument doc(256);
    doc["out1"] = digitalRead(out[0]);
    doc["out2"] = digitalRead(out[1]);
    char mqtt_message[128];
    serializeJson(doc, mqtt_message);
    publishMessage("esp32/out", mqtt_message, true);
    updateState = false;
  }
}
