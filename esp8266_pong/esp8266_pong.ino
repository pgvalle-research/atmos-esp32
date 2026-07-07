#include <ESP8266WiFi.h>
#include <espnow.h>

// Onboard active-low LED on NodeMCU (Pin D0 / GPIO16)
#define LED_PIN 16 

// =========================================================================
// CONFIGURATION: Set the MAC Address of your ESP32 board here
// =========================================================================
uint8_t esp32_mac[] = {0x24, 0x0A, 0xC4, 0x05, 0x06, 0x07}; // <--- REPLACE THIS

// Send status callback
void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Send status: ");
  Serial.println(sendStatus == 0 ? "SUCCESS" : "FAIL");
}

// Receive callback
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  // Convert incoming payload to string
  char msg[len + 1];
  memcpy(msg, incomingData, len);
  msg[len] = '\0';

  Serial.print("Received: ");
  Serial.println(msg);

  // If it's a PING, flash the LED and send back a PONG
  if (strcmp(msg, "PING") == 0) {
    digitalWrite(LED_PIN, LOW);   // LED ON (active low)
    delay(100);
    digitalWrite(LED_PIN, HIGH);  // LED OFF

    // Reply with PONG
    esp_now_send(esp32_mac, (uint8_t *)"PONG", 4);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially

  // Set Wi-Fi channel explicitly to channel 1 (standard default)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Register the ESP32 peer
  esp_now_add_peer(esp32_mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  Serial.println("ESP8266 ready, waiting for PING...");
}

void loop() {
  // FreeRTOS loop, handled by callbacks
}
