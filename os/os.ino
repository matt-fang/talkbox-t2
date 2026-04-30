#include "AudioTools.h"
#include "WiFi.h"

//
// HOLY FRICK JUST USE THIS NETCAT COMMAND (with the -p) IT WORKS
// nc -l -p 6000 | play -t raw -r 16000 -e signed -b 32 -c 1 -
// 

const char* SSID   = "bingowireless2g_EXT";
const char* PASS   = "draco10935";
// const char* remote_IP = "10.0.0.47"; // mac mini (bingowireless2g) -3/22/26
// const char* REMOTE_IP = "10.0.0.135"; // tiny talkbox (bingowireless2g_EXT) -4/20/26 | (bingowireless2g) -3/28/26
const char* REMOTE_IP = "10.0.0.154"; // big talkbox (bingowireless2g_EXT) -4/21/26
const int   REMOTE_AUDIO_PORT   = 6000;
const int   REMOTE_POT_PORT   = 5001;

const int   AUDIO_PORT = 6000;
const int   POT_PORT = 5001;

String REMOTE_AUDIO_IP_PORT_STRING = String(REMOTE_IP) + ":" + String(REMOTE_AUDIO_PORT);
String REMOTE_POT_IP_PORT_STRING = String(REMOTE_IP) + ":" + String(REMOTE_POT_PORT);

const int POT_PIN          = 34;
const int POT_PERIOD_MS = 500;

volatile unsigned long last_remote_connect_time;
bool is_connected_to_remote_server = false;
bool is_connected_to_remote_client = false;

WiFiClient audio_client;
WiFiClient pot_client;
I2SStream in;

WiFiClient remote_audio_client;
WiFiServer audio_server(AUDIO_PORT);
I2SStream out;

StreamCopy copier_in(audio_client, in, 1024);
StreamCopy copier_out(out, remote_audio_client, 1024);

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

void connectToRemote(String port = "both") {
    if (port == "audio" || port == "both") {
        while (!audio_client.connect(REMOTE_IP, REMOTE_AUDIO_PORT)) {
            Serial.println("Connecting to remote audio port... " + REMOTE_AUDIO_IP_PORT_STRING);
            delay(1000);
        }
        Serial.println("Connected to remote audio");
        last_remote_connect_time = millis();
        is_connected_to_remote_server = true;
        audio_client.setNoDelay(true);    // on sender
    }

    if (port == "pot" || port == "both") {
        while (!pot_client.connect(REMOTE_IP, REMOTE_POT_PORT)) {
            Serial.println("Connecting to remote pot port... " + REMOTE_POT_IP_PORT_STRING);
            delay(1000);
        }
        Serial.println("Connected to remote pot");
    }
}

void setupServer(String type = "both") {
    if (type == "audio" || type == "both") {
        audio_server.begin();
        Serial.println("Audio remote is live at port " + String(AUDIO_PORT));
    }

    // TODO
    // if (type == "pot" || type == "both") {
    //     pot_remote.begin();
    //     Serial.println("Pot remote is live at port " + String(POT_PORT));
    // }
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

// MARK: POT WORKS - JUST LAGGY ON remote -3/22/26
void streamPot(void * pvParameters) {
    for (;;) {
        if (!is_connected_to_remote_server) {
            Serial.println("Lost connection, reconnecting...");
            connectToRemote("pot");
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
        if (!is_connected_to_remote_server) {
            Serial.println("Lost connection, reconnecting...");
            connectToRemote("audio");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        copier_in.copy();
        last_remote_connect_time = millis();

        // Serial.println(bytesCopied);

        // if (bytesCopied <= 0) {
        //     Serial.println("Zero bytes copied, closing client connection...");
        //     audio_client.stop();
        // }
    }
}

void readSpeaker(void * pvParameters) {
    for (;;) {
        if (!remote_audio_client || !remote_audio_client.connected()) {
            remote_audio_client = audio_server.available();
            Serial.println("Waiting for audio remote client...");

            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!is_connected_to_remote_client) {
            Serial.println("Found audio client");
            is_connected_to_remote_client = true;
            remote_audio_client.setNoDelay(true);  // on receiver (set this in readSpeaker after client connects)
        }
        
        copier_out.copy();
    }
}

void manageWiFi(void * pvParameters) {
    for(;;) {
        // if (last_remote_connect_time <= millis() - 5000 && is_connected_to_remote_server) {
        //     Serial.println("Frozen copier detected, closing client connection...");
        //     audio_client.stop();
        //     is_connected_to_remote_server = false;
        // }
        

        Serial.print("wifi strength: ");
        Serial.print(WiFi.RSSI());
        Serial.print(", rx available: ");
        Serial.println(remote_audio_client.available());
        Serial.print(", connected to remote:");
        Serial.print(audio_client.connected());
        Serial.print(", remotes connected:");
        Serial.println(remote_audio_client.connected());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void setup() {
    Serial.begin(115200);

    connectToWiFi();
    
    setupMic();
    setupSpeaker();

    setupServer("audio");
    
    xTaskCreatePinnedToCore(
        streamMic,
        "Stream Mic to Remote",
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

    xTaskCreatePinnedToCore(
        readSpeaker,
        "Playback Client Mic in Speaker",
        16000,
        NULL,
        1,
        NULL,
        0
    );

    // xTaskCreatePinnedToCore(
    //     streamPot,
    //     "Stream Pot to remote",
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
