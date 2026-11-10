#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

class NTPManager {
private:
    WiFiUDP ntpUDP;
    NTPClient* timeClient;
    uint32_t last_update = 0;
    bool time_synchronized = false;

public:
    NTPManager() : timeClient(nullptr) {}

    ~NTPManager() {
        if (timeClient) delete timeClient;
    }

    void begin() {
        // UTC+7 (Western Indonesia Time / WIB) = 7 * 3600 = 25200 seconds
        timeClient = new NTPClient(ntpUDP, "id.pool.ntp.org", 25200, 60000);
        timeClient->begin();
        Serial.println(F("[NTP] Client started for UTC+7 (WIB)."));
    }

    void update() {
        if (WiFi.status() == WL_CONNECTED && timeClient) {
            if (timeClient->update()) {
                time_synchronized = true;
            }
        }
    }

    bool isSynced() const {
        return time_synchronized;
    }

    int getHours() {
        return timeClient ? timeClient->getHours() : 0;
    }

    int getMinutes() {
        return timeClient ? timeClient->getMinutes() : 0;
    }

    int getSeconds() {
        return timeClient ? timeClient->getSeconds() : 0;
    }

    unsigned long getEpochTime() {
        return timeClient ? timeClient->getEpochTime() : 0;
    }

    String getFormattedTime() {
        if (!timeClient || !time_synchronized) {
            return String(F("--:--:--"));
        }
        return timeClient->getFormattedTime();
    }
};

extern NTPManager ntpManager;
