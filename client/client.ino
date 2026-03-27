#include "AudioTools.h"
#include "WiFi.h"

const char* SSID   = "bingowireless2g";
const char* PASS   = "draco10935";
// const char* SERVER_IP = "172.20.10.2"; // t-2-1 (monkeyphone) -3/22/26
const char* SERVER_IP = "10.0.0.47"; // mac mini (bingowireless2g) -3/22/26
const int   SERVER_AUDIO_PORT   = 5000;
const int   SERVER_POT_PORT   = 5001;

String SERVER_AUDIO_IP_PORT_STRING = String(SERVER_IP) + ":" + String(SERVER_AUDIO_PORT);
String SERVER_POT_IP_PORT_STRING = String(SERVER_IP) + ":" + String(SERVER_POT_PORT);

const int POT_PIN          = 34;
const int POT_PERIOD_MS = 500;

WiFiClient audio_client;
WiFiClient pot_client;
I2SStream in;

StreamCopy copier_in(audio_client, in, 512);

void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);
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
            Serial.println("Connecting to server audio port... " + SERVER_AUDIO_IP_PORT_STRING);
            delay(1000);
        }
        Serial.println("Connected to server audio");
    }

    if (port == "pot" || port == "both") {
        while (!pot_client.connect(SERVER_IP, SERVER_POT_PORT)) {
            Serial.println("Connecting to server pot port... " + SERVER_POT_IP_PORT_STRING);
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

// MARK: POT WORKS - JUST LAGGY ON SERVER -3/22/26
void streamPot(void * pvParameters) {
    for (;;) {
        if (!pot_client.connected()) {
            Serial.println("Lost connection, reconnecting...");
            connectToServer("pot");
            vTaskDelay(pdMS_TO_TICKS(1000));
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
            connectToServer("mic");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        copier_in.copy();
    }
}

void setup() {
    Serial.begin(115200);

    connectToWiFi();
    connectToServer("audio");
    setupMic();
    
    // xTaskCreatePinnedToCore(
    //     streamMic,
    //     "Stream Mic to Server",
    //     16000,
    //     NULL,
    //     1,
    //     NULL,
    //     1
    // );

    // xTaskCreatePinnedToCore(
    //     streamPot,
    //     "Stream Pot to Server",
    //     4096,
    //     NULL,
    //     0,
    //     NULL,
    //     0
    // );
}

void loop() {

    // TEST: NO MULTITHREAD OR POT
    while (!audio_client.connected()) {
        Serial.println("Lost connection, reconnecting...");
        connectToServer("audio"); 
        
        // MARK: THIS WORKS - IT CONNECTS TO AUDIO

    }

    copier_in.copy();

    // vTaskDelete(NULL);
}
