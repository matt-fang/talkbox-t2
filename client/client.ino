#include "WiFi.h"

const char* ssid   = "monkeyphone";
const char* pass   = "password";
const char* server = "172.20.10.2";
const int   port   = 5000;

const int POT_PIN          = 34;
const int SEND_INTERVAL_MS = 50;

WiFiClient client;

void connectToServer() {
    while (!client.connect(server, port)) {
        Serial.println("Retrying server...");
        delay(1000);
    }
    Serial.println("Connected to server");
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi...");
        delay(500);
    }
    Serial.println("WiFi connected");

    connectToServer();
}

void loop() {
    if (!client.connected()) {
        Serial.println("Lost connection, reconnecting...");
        connectToServer();
        return;
    }

    float potNorm = 1.0f - (analogRead(POT_PIN) / 4095.0f); // reversed: CW = low
    client.println(potNorm);

    delay(SEND_INTERVAL_MS);
}
