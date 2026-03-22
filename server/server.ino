#include "WiFi.h"
#include "FastLED.h"

#define NUM_LEDS    60
#define LED_PIN     23
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

const char* ssid = "monkeyphone";
const char* pass = "password";
const int   port = 5000;

const int POT_PIN = 34;

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

// ── TCP server ─────────────────────────────────────────────
WiFiServer  tcpServer(port);
WiFiClient  remoteClient;
String      rxBuffer   = "";
float       remotePot  = 0.0f;   // received from client

// ── Height map ────────────────────────────────────────────
void buildHeightMap() {
    for (int i = 0; i < NUM_LEDS; i++) ledHeight[i] = -1.0f;

    for (int i = 0;  i <= 4;  i++) ledHeight[i] = 0.0f;
    for (int i = 29; i <= 33; i++) ledHeight[i] = 0.0f;
    for (int i = 5;  i <= 12; i++) ledHeight[i] = (i - 4) / 9.0f;
    for (int i = 13; i <= 20; i++) ledHeight[i] = 1.0f;
    for (int i = 21; i <= 28; i++) ledHeight[i] = (29 - i) / 9.0f;
}

// ── Pulse brightness driven by remote pot ─────────────────
// remotePot = 0 → solid at BRIGHT_MAX
// remotePot = 1 → oscillates BRIGHT_MIN↔BRIGHT_MAX at MAX_PULSE_HZ
uint8_t pulseBrightness() {
    float freq   = remotePot * MAX_PULSE_HZ;
    float sine   = (sinf(millis() / 1000.0f * freq * TWO_PI) + 1.0f) * 0.5f; // 0–1
    float bright = BRIGHT_MAX - remotePot * (BRIGHT_MAX - BRIGHT_MIN) * (1.0f - sine);
    return (uint8_t)bright;
}

// ── LED update ────────────────────────────────────────────
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

// ── TCP: non-blocking read of remote pot ─────────────────
void readRemotePot() {
    // Accept new client if none connected
    if (!remoteClient || !remoteClient.connected()) {
        remoteClient = tcpServer.available();
        return;
    }

    // Accumulate chars; parse float on newline
    while (remoteClient.available()) {
        char c = remoteClient.read();
        if (c == '\n') {
            remotePot = rxBuffer.toFloat();
            rxBuffer  = "";
        } else if (c != '\r') {
            rxBuffer += c;
        }
    }
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHT_MIN);
    buildHeightMap();
    fill_solid(leds,      NUM_LEDS, CRGB::Black);
    fill_solid(ledTarget, NUM_LEDS, CRGB::Black);
    FastLED.show();

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi...");
        delay(500);
    }
    Serial.print("Server IP: ");
    Serial.println(WiFi.localIP());

    tcpServer.begin();
    Serial.println("TCP server listening on port 5000");
}

// ── Loop ──────────────────────────────────────────────────
void loop() {
    readRemotePot();
    float localPot = 1.0f - (analogRead(POT_PIN) / 4095.0f); // reversed: CW = low
    updateLEDs(localPot);
}
