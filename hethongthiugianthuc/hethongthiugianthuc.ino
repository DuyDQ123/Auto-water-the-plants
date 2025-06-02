#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define DHTPIN 14
#define DHTTYPE DHT11
#define RELAY_PIN 4
#define BUTTON_PIN 27
#define HUMIDITY_THRESHOLD 60

// WiFi credentials
const char* ssid = "duy";
const char* password = "11111111";

// Server details
const char* serverUrl = "http://192.168.137.1:5000"; 

DHT dht(DHTPIN, DHTTYPE);
float currentHumidity = 0.0;
float currentTemperature = 0.0;

bool manualOverride = false; // biến để kiểm soát trạng thái bơm bằng tay
bool manualPumpState = false; // biến để lưu trạng thái bơm khi điều khiển bằng tay
unsigned long manualStartTime = 0; // thời gian bắt đầu điều khiển bằng tay
const unsigned long manualTimeout = 10000; // 10 giây

void sendSensorData() {
  HTTPClient http;
  
  // Create JSON object
  StaticJsonDocument<200> doc;
  doc["temperature"] = currentTemperature;
  doc["humidity"] = currentHumidity;
  doc["pump_state"] = (digitalRead(RELAY_PIN) == LOW);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send POST request
  http.begin(String(serverUrl) + "/update_sensor");
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST success: %d\n", httpResponseCode);
  } else {
    Serial.printf("[HTTP] POST failed: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
}

void ReadSensorTask(void *pvParameters) {
  for (;;) {//
    currentHumidity = dht.readHumidity();
    currentTemperature = dht.readTemperature();

    if (!isnan(currentHumidity) && !isnan(currentTemperature)) {
      Serial.printf("[Sensor] Temp: %.1f°C | Humi: %.1f%%\n", currentTemperature, currentHumidity);
      sendSensorData();
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void checkWebCommand() {
  HTTPClient http;
  http.begin(String(serverUrl) + "/check_command");
  
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String response = http.getString();
    StaticJsonDocument<200> doc;// cân chỉnh kích thước nếu cần
    DeserializationError error = deserializeJson(doc, response);//
    
    if (!error && doc["has_command"]) {
      bool command = doc["command"];
      manualOverride = true;
      manualPumpState = command;
      manualStartTime = millis();
      Serial.printf("[Web] Received command: %s\n", command ? "BẬT" : "TẮT");
    }
  }
  
  http.end();
}

void ControlRelayTask(void *pvParameters) {
  for (;;) {
    if (!manualOverride) {
      checkWebCommand();  // Check for web commands when not in manual override
    }

    if (manualOverride) {
      digitalWrite(RELAY_PIN, manualPumpState ? LOW : HIGH);
      Serial.println("[Relay] Điều khiển bằng TAY!");

      if (millis() - manualStartTime >= manualTimeout) {
        manualOverride = false;
        Serial.println("[Relay] Hết 10s → Trở lại chế độ TỰ ĐỘNG");
        manualPumpState = (currentHumidity < HUMIDITY_THRESHOLD);
      }
    } else {
      if (currentHumidity < HUMIDITY_THRESHOLD) {
        digitalWrite(RELAY_PIN, LOW); // Bật bơm
        Serial.println("[Relay] Độ ẩm thấp → BẬT bơm");
      } else {
        digitalWrite(RELAY_PIN, HIGH); // Tắt bơm
        Serial.println("[Relay] Độ ẩm đủ → TẮT bơm");
      }
      manualPumpState = (digitalRead(RELAY_PIN) == LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void ButtonTask(void *pvParameters) {
  bool lastState = HIGH;
  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 200; // 200ms debounce time

  for (;;) {
    bool currentState = digitalRead(BUTTON_PIN);

    if (lastState == HIGH && currentState == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        manualOverride = true;
        manualPumpState = !manualPumpState;
        manualStartTime = millis();
        
        // Directly control relay
        digitalWrite(RELAY_PIN, manualPumpState ? LOW : HIGH);

        Serial.printf("[Button] Nhấn nút → Chuyển trạng thái bơm thành: %s\n",
                    manualPumpState ? "BẬT" : "TẮT");
                    
        lastDebounceTime = millis();
      }
    }

    lastState = currentState;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Tắt bơm ban đầu

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Button task highest priority
  xTaskCreate(ButtonTask, "Button", 2048, NULL, 3, NULL);
  
  // Web control and relay control medium priority
  xTaskCreate(ControlRelayTask, "ControlRelay", 4096, NULL, 2, NULL);
  
  // Sensor reading lowest priority
  xTaskCreate(ReadSensorTask, "ReadSensor", 8192, NULL, 1, NULL);
}

void loop() {
  // Không cần dùng loop
}
