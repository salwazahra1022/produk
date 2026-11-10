#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include "config_manager.h"
#include "ntp_manager.h"
#include "led_effects.h"
#include "web_server.h"

// Instantiate Global Managers
ConfigManager config;
NTPManager ntpManager;
LEDController ledController;
WebServerManager webServerManager;

unsigned long last_diag_broadcast = 0;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  Smart LED WS2812B Controller - ESP8266  "));
    Serial.println(F("=========================================="));

    // 1. Initialize LittleFS and Load Configuration
    if (!config.begin()) {
        Serial.println(F("[Setup] Using default config."));
    }

    // 2. Initialize FastLED hardware
    ledController.begin();

    // 3. Connect to WiFi via WiFiManager Captive Portal
    WiFi.hostname("smartled");
    WiFiManager wm;
    wm.setConfigPortalTimeout(180); // 3 minutes timeout if not configured

    Serial.println(F("[WiFi] Connecting or starting AP 'SmartLED-Setup'..."));
    bool connected = wm.autoConnect("SmartLED-Setup");

    if (!connected) {
        Serial.println(F("[WiFi] Timeout reached. Operating in standalone AP mode."));
        WiFi.mode(WIFI_AP);
        WiFi.softAP("SmartLED-Setup");
        Serial.print(F("[WiFi] AP IP: "));
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.print(F("[WiFi] Connected! IP Address: "));
        Serial.println(WiFi.localIP());
    }

    // 4. Start mDNS Responder (http://smartled.local)
    if (MDNS.begin("smartled")) {
        Serial.println(F("[mDNS] Responder started at http://smartled.local"));
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println(F("[mDNS] Failed to start responder."));
    }

    // 5. Start NTP Client
    if (WiFi.status() == WL_CONNECTED) {
        ntpManager.begin();
    }

    // 6. Start AsyncWebServer, WebSocket, and ElegantOTA
    webServerManager.begin();

    Serial.println(F("[Setup] System Ready! Enjoy your Smart LED."));
}

void loop() {
    // Keep mDNS active
    MDNS.update();

    // ElegantOTA background loop
    ElegantOTA.loop();

    // LED Animation & Effect processor
    ledController.update();

    // If timer or schedule toggled power state, notify all connected clients
    if (ledController.checkStateChanged()) {
        webServerManager.broadcastState();
    }

    // Handle debounced config saving to flash LittleFS
    config.handleDebounce();

    // Cleanup disconnected WebSocket clients
    webServerManager.cleanupClients();

    // NTP periodic sync
    static unsigned long last_ntp_poll = 0;
    if (millis() - last_ntp_poll >= 15000) {
        last_ntp_poll = millis();
        ntpManager.update();
    }

    // Broadcast diagnostic telemetry (Heap, RSSI, Clock) every 3 seconds to active WS clients
    if (millis() - last_diag_broadcast >= 3000) {
        last_diag_broadcast = millis();
        webServerManager.sendStatus(nullptr);
    }

    // Non-blocking yield for ESP8266 background WiFi stack
    yield();
}
