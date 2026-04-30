#include "AudioTools.h"
#include "WiFi.h"
#include "WiFiUdp.h"

#define NUM_LEDS    60
#define LED_PIN     23
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

const char* SSID = "bingowireless2g_EXT";
const char* PASS = "draco10935";
const int   AUDIO_PORT = 6000;

WiFiUDP udp;
I2SStream out;

int packet_size;

uint8_t buffer[512];

void connectToWiFi() {
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi...");
        Serial.println(WiFi.RSSI());
        delay(2000);
    }
    Serial.println("WiFi connected at IP:");
    Serial.println(WiFi.localIP());
}

void setupSpeaker() {
    auto cfg_out = out.defaultConfig(TX_MODE);
    cfg_out.sample_rate = 16000;
    cfg_out.bits_per_sample = 32;
    cfg_out.channels = 1;
    cfg_out.pin_bck = 26;
    cfg_out.pin_ws = 25; // lrck
    cfg_out.pin_data = 22; // sd
    cfg_out.port_no = 1;

    out.begin(cfg_out);

    Serial.println("Speaker is running");

    udp.begin(AUDIO_PORT);
    Serial.println("UDP port is open.");
}

void readSpeaker(void * pvParameters) {
    for (;;) {
        if (packet_size = udp.parsePacket()) {
            udp.read(buffer, 512);
            out.write(buffer, packet_size);
        }
    }
}

void setup() {
    Serial.begin(115200);

    connectToWiFi();
    setupSpeaker();

    xTaskCreatePinnedToCore(
        readSpeaker,
        "Read Client Mic to Speaker",
        16000,
        NULL,
        1,
        NULL,
        1
    );

    // xTaskCreatePinnedToCore(
    //     readPot,
    //     "Read Client Pot to LEDs",
    //     16000,
    //     NULL,
    //     0,
    //     NULL,
    //     1
    // );
}

void loop() {
    vTaskDelete(NULL);
}
