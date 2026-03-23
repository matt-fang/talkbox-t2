#include "AudioTools.h"
#include "WiFi.h"
#include "FastLED.h"

#define NUM_LEDS    60
#define LED_PIN     23
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

const char* SSID = "monkeyphone";
const char* PASS = "password";
const int   AUDIO_PORT = 5000;
const int   POT_PORT = 5001;

const int POT_PIN = 34;
const int POT_PERIOD_MS = 500;

// ── LED constants ──────────────────────────────────────────
const CRGB COLOR_AMBER = CRGB(0xFF, 0xBA, 0x08);
const CRGB COLOR_SKY   = CRGB(0xC9 / 3, 0xEE / 3, 0xF8 / 3);
const CRGB COLOR_WHITE = CRGB(255, 255, 255);

const uint8_t BRIGHT_MIN    = 1;
uint8_t BRIGHT_MAX    = 64;
const float   MAX_THRESHOLD = 0.96f;
const uint8_t FADE_AMOUNT   = 60;
const float   MAX_PULSE_HZ  = 1.5f;

CRGB  leds[NUM_LEDS];
CRGB  ledTarget[NUM_LEDS];
float ledHeight[NUM_LEDS];

WiFiServer audio_server(AUDIO_PORT);
WiFiServer pot_server(POT_PORT);
WiFiClient audio_client;
WiFiClient pot_client;
I2SStream out;

StreamCopy copier_out(out, audio_client, 256);
// TODO: add volume stream

String      rxBuffer   = "";
float       remotePot  = 0.0f;   // received from client

void connectToWiFi() {
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi...");
        delay(500);
    }
    Serial.println("WiFi connected");
}

void setupServer(String type = "both") {
    if (type == "audio" || type == "both") {
        audio_server.begin();
        Serial.println("Audio server is live at port " + String(AUDIO_PORT));
    }

    if (type == "pot" || type == "both") {
        pot_server.begin();
        Serial.println("Pot server is live at port " + String(POT_PORT));
    }
}

void setupLEDs() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHT_MIN);
    buildHeightMap();
    fill_solid(leds,      NUM_LEDS, CRGB::Black);
    fill_solid(ledTarget, NUM_LEDS, CRGB::Black);
    FastLED.show();

    Serial.println("LEDs are running");
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
}

// MARK: LED
// ---------------------------------------------------------

void buildHeightMap() {
    for (int i = 0; i < NUM_LEDS; i++) ledHeight[i] = -1.0f;

    for (int i = 0;  i <= 4;  i++) ledHeight[i] = 0.0f;
    for (int i = 29; i <= 33; i++) ledHeight[i] = 0.0f;
    for (int i = 5;  i <= 12; i++) ledHeight[i] = (i - 4) / 9.0f;
    for (int i = 13; i <= 20; i++) ledHeight[i] = 1.0f;
    for (int i = 21; i <= 28; i++) ledHeight[i] = (29 - i) / 9.0f;
}

uint8_t pulseBrightness() {
    float freq   = remotePot * MAX_PULSE_HZ;
    float sine   = (sinf(millis() / 1000.0f * freq * TWO_PI) + 1.0f) * 0.5f; // 0–1
    float bright = BRIGHT_MAX - remotePot * (BRIGHT_MAX - BRIGHT_MIN) * (1.0f - sine);
    return (uint8_t)bright;
}

void updateLEDs(float localPot) {
    BRIGHT_MAX = localPot * 64;
    FastLED.setBrightness(pulseBrightness());

    bool maxed = localPot >= MAX_THRESHOLD;

    for (int i = 0; i < NUM_LEDS; i++) {
        if      (ledHeight[i] < 0.0f) ledTarget[i] = CRGB::Black;
        else if (maxed)                ledTarget[i] = COLOR_WHITE;
        else ledTarget[i] = (ledHeight[i] <= localPot) ? COLOR_AMBER : COLOR_SKY;
    }

    for (int i = 0; i < NUM_LEDS; i++) nblend(leds[i], ledTarget[i], FADE_AMOUNT);

    FastLED.show();
}

// ---------------------------------------------------------

void readPot(void * pvParameters) {
    for (;;) {
        if (!pot_client || !pot_client.connected()) {
            pot_client = pot_server.available();
            
            Serial.println("Waiting for pot client...");
            continue;
        }

        Serial.println("Found pot client");

        while (pot_client.available()) {
            char c = pot_client.read();
            if (c == '\n') {
                remotePot = rxBuffer.toFloat();
                rxBuffer  = "";
            } else if (c != '\r') {
                rxBuffer += c;
            }

            float localPot = 1.0f - (analogRead(POT_PIN) / 4095.0f); // reversed: CW = low
            updateLEDs(localPot);
        }

        vTaskDelay(pdMS_TO_TICKS(POT_PERIOD_MS)); // TODO: will it sync with client
    }
    
}

void readSpeaker(void * pvParameters) {
    for (;;) {
        if (!audio_client || !audio_client.connected()) {
            audio_client = audio_server.available();

            Serial.println("Waiting for audio client...");
            continue;
        }

        Serial.println("Found audio client");

        copier_out.copy();

        // TODO: do we need vtaskdelay
    }
}

void setup() {
    Serial.begin(115200);

    connectToWiFi();
    setupLEDs();
    setupServer("both");

    xTaskCreatePinnedToCore(
        readSpeaker,
        "Read Client Mic to Speaker",
        16000,
        NULL,
        1,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        readPot,
        "Read Client Pot to LEDs",
        16000,
        NULL,
        0,
        NULL,
        1
    );
}

void loop() {
    vTaskDelete(NULL);
}
