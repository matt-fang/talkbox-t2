#include "AudioTools.h"
#include "WiFi.h"
#include "WiFiUdp.h"

//
// HOLY FRICK JUST USE THIS NETCAT COMMAND (with the -p) IT WORKS
// nc -l -p 6000 | play -t raw -r 16000 -e signed -b 32 -c 1 -
//

const char* SSID              = "bingowireless2g_EXT";
const char* PASS              = "draco10935";
// const char* REMOTE_IP      = "10.0.0.47";  // mac mini (bingowireless2g) -3/22/26
const char* REMOTE_IP      = "10.0.0.135"; // tiny talkbox (bingowireless2g_EXT) -4/20/26 | (bingowireless2g) -3/28/26
// const char* REMOTE_IP         = "10.0.0.154"; // big talkbox (bingowireless2g_EXT) -4/21/26
const int   REMOTE_AUDIO_PORT = 6000;
const int   REMOTE_POT_PORT   = 5001;
const int   AUDIO_PORT        = 6000;
const int   POT_PORT          = 5001;

const int   POT_PIN       = 34;
const int   POT_PERIOD_MS = 500;

WiFiUDP   udp_out;
WiFiUDP   udp_in;
I2SStream in;
I2SStream out;

uint8_t tx_buffer[512];
uint8_t rx_buffer[512];

volatile int tx_bytes = 0;
volatile int rx_bytes = 0;

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

    WiFi.setSleep(WIFI_PS_NONE);

    Serial.println("WiFi connected at IP:");
    Serial.println(WiFi.localIP());
}

void setupMic() {
    auto cfg_in = in.defaultConfig(RX_MODE);
    cfg_in.sample_rate     = 16000;
    cfg_in.bits_per_sample = 32;
    cfg_in.channels        = 1;
    cfg_in.pin_bck         = 32;
    cfg_in.pin_ws          = 15;
    cfg_in.pin_data        = 33;
    cfg_in.port_no         = 0;
    in.begin(cfg_in);
    Serial.println("Mic is running");
}

void setupSpeaker() {
    auto cfg_out = out.defaultConfig(TX_MODE);
    cfg_out.sample_rate     = 16000;
    cfg_out.bits_per_sample = 32;
    cfg_out.channels        = 1;
    cfg_out.pin_bck         = 26;
    cfg_out.pin_ws          = 25; // lrck
    cfg_out.pin_data        = 22; // sd
    cfg_out.port_no         = 1;
    out.begin(cfg_out);
    Serial.println("Speaker is running");

    udp_in.begin(AUDIO_PORT);
    Serial.println("UDP listening on port " + String(AUDIO_PORT));
}

void streamMic(void* pvParameters) {
    for (;;) {
        int byte_size = in.readBytes(tx_buffer, 512);
        if (byte_size) {
            udp_out.beginPacket(REMOTE_IP, REMOTE_AUDIO_PORT);
            udp_out.write(tx_buffer, byte_size);
            tx_bytes += byte_size;
            udp_out.endPacket();
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void readSpeaker(void* pvParameters) {
    for (;;) {
        int packet_size = udp_in.parsePacket();
        if (packet_size) {
            udp_in.read(rx_buffer, 512);
            rx_bytes += packet_size;
            out.write(rx_buffer, packet_size);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void manageWiFi(void* pvParameters) {
    for (;;) {
        Serial.print("wifi strength: ");
        Serial.println(WiFi.RSSI());
        Serial.printf("tx: %d bytes/sec, rx: %d bytes/sec\n", tx_bytes, rx_bytes);
        tx_bytes = 0;
        rx_bytes = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// void streamPot(void* pvParameters) {
//     for (;;) {
//         float potNorm = 1.0f - (analogRead(POT_PIN) / 4095.0f); // reversed: CW = low
//         // TODO: convert to UDP
//         vTaskDelay(pdMS_TO_TICKS(POT_PERIOD_MS));
//     }
// }

void setup() {
    Serial.begin(115200);

    connectToWiFi();
    setupMic();
    setupSpeaker();

    xTaskCreatePinnedToCore(streamMic,   "Stream Mic to Remote",          16000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(manageWiFi,  "WiFi Diagnostics",               8000, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(readSpeaker, "Playback Remote to Speaker", 16000, NULL, 1, NULL, 0);

    // xTaskCreatePinnedToCore(streamPot, "Stream Pot to Remote", 4096, NULL, 0, NULL, 0);
}

void loop() {
    vTaskDelete(NULL);
}
