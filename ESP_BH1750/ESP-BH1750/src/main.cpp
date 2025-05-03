#include <WiFi.h>
#include <Wire.h>
#include <BH1750.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// WiFi & MQTT cấu hình
const char* ssid = "Tầng 2";
const char* password = "lacaigivay";
const char* mqtt_server = "f8e9523a8a3b48ea957832a96e91caa0.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_username = "e-smart";
const char* mqtt_password = "Abc@1234";

// MQTT client qua TLS
WiFiClientSecure espClient;
PubSubClient client(espClient);

// GPIO output cho LED
const int outPins[2] = {25, 33};
bool updateState = false;

// BH1750 I2C trên chân SDA=32, SCL=26
#define I2C_SDA_PIN 32
#define I2C_SCL_PIN 26
BH1750 lightMeter;

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
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("MQTT nhận [" + String(topic) + "]: " + message);

  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, message)) {
    Serial.println("Lỗi parse JSON");
    return;
  }

  if (doc.containsKey("out1")) {
    bool p = doc["out1"];
    digitalWrite(outPins[0], p);
    Serial.println("out1 = " + String(p));
  }
  if (doc.containsKey("out2")) {
    bool p = doc["out2"];
    digitalWrite(outPins[1], p);
    Serial.println("out2 = " + String(p));
  }
  updateState = true;
}

void publishMessage(const char* topic, const String& payload, bool retained) {
  if (client.publish(topic, payload.c_str(), retained)) {
    Serial.println("Đã gửi [" + String(topic) + "]: " + payload);
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();

  // MQTT qua TLS
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Cấu hình chân LED
  for (int i = 0; i < 2; i++) {
    pinMode(outPins[i], OUTPUT);
    digitalWrite(outPins[i], LOW);
  }

  // Khởi tạo I2C cho BH1750 (SDA=32, SCL=26)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("Không tìm thấy BH1750 trên I2C!");
    while (1) { delay(1000); }
  }
  Serial.println("Đã kết nối BH1750 (I2C SDA=32, SCL=26).");
}

unsigned long lastReadTime = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Đọc BH1750 mỗi 1s
  if (millis() - lastReadTime > 1000) {
    float lux = lightMeter.readLightLevel();
    if (lux >= 0) {
      DynamicJsonDocument doc(256);
      doc["lux"] = lux;
      char buf[128];
      serializeJson(doc, buf);
      publishMessage("esp32/bh1750", String(buf), true);
      Serial.println("Gửi lux: " + String(lux) + " lux");
    } else {
      Serial.println("Đọc BH1750 lỗi");
    }
    lastReadTime = millis();
  }

  // Gửi trạng thái LED khi có thay đổi
  if (updateState) {
    DynamicJsonDocument doc(256);
    doc["out1"] = (bool)digitalRead(outPins[0]);
    doc["out2"] = (bool)digitalRead(outPins[1]);
    char buf[128];
    serializeJson(doc, buf);
    publishMessage("esp32/out", String(buf), true);
    updateState = false;
  }
}
