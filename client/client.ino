#include "AudioTools.h"
#include "WiFi.h"

const char* SSID   = "monkeyphone";
const char* PASS   = "password";
const char* SERVER_IP = "172.20.10.2"; // THIS GOOD! DONT CHANGE -3/22/26
const int   SERVER_AUDIO_PORT   = 5000;
const int   SERVER_POT_PORT   = 5001;

String SERVER_AUDIO_IP_PORT_STRING = String(SERVER_IP) + ":" + String(SERVER_AUDIO_PORT);
String SERVER_POT_IP_PORT_STRING = String(SERVER_IP) + ":" + String(SERVER_POT_PORT);

const int POT_PIN          = 34;
const int POT_PERIOD_MS = 500;

WiFiClient audio_client;
WiFiClient pot_client;
I2SStream in;

StreamCopy copier_in(audio_client, in, 256);

void connectToWiFi() {
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi...");
        delay(500);
    }
    Serial.println("WiFi connected");
}

void connectToServer(String port = "both") {
    if (port == "audio" || port == "both") {
        while (!audio_client.connect(SERVER_IP, SERVER_AUDIO_PORT)) {
            Serial.println("Retrying server audio... " + SERVER_AUDIO_IP_PORT_STRING);
            delay(1000);
        }
        Serial.println("Connected to server audio");
    }

    if (port == "pot" || port == "both") {
        while (!pot_client.connect(SERVER_IP, SERVER_POT_PORT)) {
            Serial.println("Retrying server pot... " + SERVER_POT_IP_PORT_STRING);
            delay(1000);
        }
        Serial.println("Connected to server pot");
    }
}

void setupMic() {
    auto cfg_in = in.defaultConfig(RX_MODE);
    cfg_in.sample_rate = 16000;
    cfg_in.bits_per_sample = 32;
    cfg_in.channels = 1;
    cfg_in.pin_bck = 32;
    cfg_in.pin_ws = 15;
    cfg_in.pin_data = 33;
    cfg_in.port_no = 0;

    in.begin(cfg_in);

    Serial.println("Mic is running");
}

void streamPot(void * pvParameters) {
    for (;;) {
        if (!pot_client.connected()) {
            Serial.println("Lost connection, reconnecting...");
            connectToServer();
            continue;
        }

        float potNorm = 1.0f - (analogRead(POT_PIN) / 4095.0f); // reversed: CW = low
        pot_client.println(potNorm);

        vTaskDelay(pdMS_TO_TICKS(POT_PERIOD_MS));
    }
}

void streamMic(void * pvParameters) {
    for (;;) {
        if (!audio_client.connected()) {
            Serial.println("Lost connection, reconnecting...");
            connectToServer();
            continue;
        }
        copier_in.copy();
    }
}

void setup() {
    Serial.begin(115200);

    connectToWiFi();
    connectToServer("both");
    setupMic();
    
    xTaskCreatePinnedToCore(
        streamMic,
        "Stream Mic to Server",
        16000,
        NULL,
        1,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        streamPot,
        "Stream Pot to Server",
        4096,
        NULL,
        0,
        NULL,
        0
    );
}

void loop() {
    vTaskDelete(NULL);
}
