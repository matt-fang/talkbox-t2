#include "AudioTools.h"
#include "WiFi.h"

//
// HOLY FRICK JUST USE THIS NETCAT COMMAND (with the -p) IT WORKS
// nc -l -p 6000 | play -t raw -r 16000 -e signed -b 32 -c 1 -
// 

const char* SSID   = "bingowireless2g_EXT";
const char* PASS   = "draco10935";
// const char* SERVER_IP = "10.0.0.47"; // mac mini (bingowireless2g) -3/22/26
const char* SERVER_IP = "10.0.0.135"; // tiny talkbox (bingowireless2g_EXT) -4/20/26 | (bingowireless2g) -3/28/26
const int   SERVER_AUDIO_PORT   = 6000;
const int   SERVER_POT_PORT   = 5001;

String SERVER_AUDIO_IP_PORT_STRING = String(SERVER_IP) + ":" + String(SERVER_AUDIO_PORT);
String SERVER_POT_IP_PORT_STRING = String(SERVER_IP) + ":" + String(SERVER_POT_PORT);

const int POT_PIN          = 34;
const int POT_PERIOD_MS = 500;

volatile unsigned long last_server_connect_time;
bool is_connected_to_server = false;

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
        Serial.println(WiFi.RSSI());
        delay(2000);
    }
    Serial.println("WiFi connected at IP:");
    Serial.println(WiFi.localIP());
}

void connectToServer(String port = "both") {
    if (port == "audio" || port == "both") {
        while (!audio_client.connect(SERVER_IP, SERVER_AUDIO_PORT)) {
            Serial.println("Connecting to server audio port... " + SERVER_AUDIO_IP_PORT_STRING);
            delay(1000);
        }
        Serial.println("Connected to server audio");
        last_server_connect_time = millis();
        is_connected_to_server = true;
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
        if (!is_connected_to_server) {
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
        if (!is_connected_to_server) {
            Serial.println("Lost connection, reconnecting...");
            connectToServer("audio");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        copier_in.copy();
        last_server_connect_time = millis();

        // Serial.println(bytesCopied);

        // if (bytesCopied <= 0) {
        //     Serial.println("Zero bytes copied, closing client connection...");
        //     audio_client.stop();
        // }
    }
}

void manageWiFi(void * pvParameters) {
    for(;;) {
        if (last_server_connect_time <= millis() - 5000 && is_connected_to_server) {
            Serial.println("Frozen copier detected, closing client connection...");
            audio_client.stop();
            is_connected_to_server = false;
        }

        Serial.print("wifi strength: ");
        Serial.print(WiFi.RSSI());
        Serial.print(", connected:");
        Serial.println(audio_client.connected());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void setup() {
    Serial.begin(115200);

    connectToWiFi();
    connectToServer("audio");
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
        manageWiFi,
        "Manage TCP Connection + Diagonistics",
        8000,
        NULL,
        0,
        NULL,
        1
    );

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
    vTaskDelete(NULL);
}
