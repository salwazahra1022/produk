#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define MAX_LEDS_LIMIT 300
#define CONFIG_FILE "/config.json"
#define DEBOUNCE_DELAY_MS 2000

struct ColorRGB {
    uint8_t r = 255;
    uint8_t g = 0;
    uint8_t b = 150;
};

struct GradientStop {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct TimerConfig {
    bool active = false;
    uint32_t duration_sec = 0;
    uint32_t start_ms = 0;
    bool gradual_dim = false;
    uint8_t initial_brightness = 150;
};

struct ScheduleConfig {
    bool enabled = false;
    uint8_t on_hour = 18;
    uint8_t on_min = 0;
    uint8_t off_hour = 6;
    uint8_t off_min = 0;
    int last_triggered_min = -1;
};

class ConfigManager {
public:
    // Core LED settings
    bool power = true;
    uint8_t brightness = 150; // 0 - 255
    ColorRGB color = {255, 0, 150}; // Cyber Neon Pink default
    uint16_t color_temp = 4000; // 2700K - 6500K
    uint8_t effect_mode = 0; // 0 = Solid
    uint8_t speed = 50; // 1 - 100
    uint8_t intensity = 50; // 1 - 100
    
    // Hardware configuration
    uint16_t num_leds = 30;
    uint16_t max_ma = 1500;

    // Custom gradient stops (2 - 4)
    uint8_t gradient_count = 2;
    GradientStop gradient[4] = {
        {255, 0, 128},   // Pink
        {0, 240, 255},   // Cyan
        {120, 0, 255},   // Purple
        {255, 180, 0}    // Amber
    };

    // Timer & Schedule
    TimerConfig timer;
    ScheduleConfig schedule;

    // Debounce save state
    bool is_dirty = false;
    uint32_t last_change_ms = 0;

    bool begin() {
        if (!LittleFS.begin()) {
            Serial.println(F("[LittleFS] Mounting failed, formatting..."));
            LittleFS.format();
            if (!LittleFS.begin()) {
                Serial.println(F("[LittleFS] Mount failed even after format!"));
                return false;
            }
        }
        Serial.println(F("[LittleFS] Mounted successfully."));
        return load();
    }

    void markDirty() {
        is_dirty = true;
        last_change_ms = millis();
    }

    void handleDebounce() {
        if (is_dirty && (millis() - last_change_ms >= DEBOUNCE_DELAY_MS)) {
            save();
            is_dirty = false;
        }
    }

    bool load() {
        if (!LittleFS.exists(CONFIG_FILE)) {
            Serial.println(F("[Config] No config file found. Using defaults."));
            save();
            return false;
        }

        File file = LittleFS.open(CONFIG_FILE, "r");
        if (!file) {
            Serial.println(F("[Config] Failed to open config file for reading."));
            return false;
        }

        StaticJsonDocument<1536> doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.printf("[Config] Deserialization error: %s\n", error.c_str());
            return false;
        }

        power = doc["power"] | true;
        brightness = doc["brightness"] | 150;
        color.r = doc["color"]["r"] | 255;
        color.g = doc["color"]["g"] | 0;
        color.b = doc["color"]["b"] | 150;
        color_temp = doc["color_temp"] | 4000;
        effect_mode = doc["effect_mode"] | 0;
        speed = doc["speed"] | 50;
        intensity = doc["intensity"] | 50;
        num_leds = doc["num_leds"] | 30;
        if (num_leds > MAX_LEDS_LIMIT) num_leds = MAX_LEDS_LIMIT;
        if (num_leds < 1) num_leds = 1;
        max_ma = doc["max_ma"] | 1500;
        if (max_ma < 200) max_ma = 200;

        if (doc.containsKey("gradient") && doc["gradient"].is<JsonArray>()) {
            JsonArray arr = doc["gradient"];
            gradient_count = arr.size();
            if (gradient_count > 4) gradient_count = 4;
            if (gradient_count < 2) gradient_count = 2;
            for (size_t i = 0; i < gradient_count; i++) {
                gradient[i].r = arr[i]["r"] | 0;
                gradient[i].g = arr[i]["g"] | 0;
                gradient[i].b = arr[i]["b"] | 0;
            }
        }

        if (doc.containsKey("schedule")) {
            JsonObject sch = doc["schedule"];
            schedule.enabled = sch["enabled"] | false;
            schedule.on_hour = sch["on_hour"] | 18;
            schedule.on_min = sch["on_min"] | 0;
            schedule.off_hour = sch["off_hour"] | 6;
            schedule.off_min = sch["off_min"] | 0;
        }

        Serial.println(F("[Config] Configuration loaded successfully."));
        return true;
    }

    bool save() {
        StaticJsonDocument<1536> doc;
        doc["power"] = power;
        doc["brightness"] = brightness;

        JsonObject c = doc.createNestedObject("color");
        c["r"] = color.r;
        c["g"] = color.g;
        c["b"] = color.b;

        doc["color_temp"] = color_temp;
        doc["effect_mode"] = effect_mode;
        doc["speed"] = speed;
        doc["intensity"] = intensity;
        doc["num_leds"] = num_leds;
        doc["max_ma"] = max_ma;

        JsonArray arr = doc.createNestedArray("gradient");
        for (uint8_t i = 0; i < gradient_count; i++) {
            JsonObject stop = arr.createNestedObject();
            stop["r"] = gradient[i].r;
            stop["g"] = gradient[i].g;
            stop["b"] = gradient[i].b;
        }

        JsonObject sch = doc.createNestedObject("schedule");
        sch["enabled"] = schedule.enabled;
        sch["on_hour"] = schedule.on_hour;
        sch["on_min"] = schedule.on_min;
        sch["off_hour"] = schedule.off_hour;
        sch["off_min"] = schedule.off_min;

        File file = LittleFS.open(CONFIG_FILE, "w");
        if (!file) {
            Serial.println(F("[Config] Failed to open config file for writing."));
            return false;
        }

        serializeJson(doc, file);
        file.close();
        Serial.println(F("[Config] Saved to LittleFS (debounced)."));
        return true;
    }

    String getStateJson() {
        StaticJsonDocument<1536> doc;
        doc["type"] = "state";
        doc["power"] = power;
        doc["brightness"] = brightness;

        JsonObject c = doc.createNestedObject("color");
        c["r"] = color.r;
        c["g"] = color.g;
        c["b"] = color.b;

        doc["color_temp"] = color_temp;
        doc["effect_mode"] = effect_mode;
        doc["speed"] = speed;
        doc["intensity"] = intensity;
        doc["num_leds"] = num_leds;
        doc["max_ma"] = max_ma;

        JsonArray arr = doc.createNestedArray("gradient");
        for (uint8_t i = 0; i < gradient_count; i++) {
            JsonObject stop = arr.createNestedObject();
            stop["r"] = gradient[i].r;
            stop["g"] = gradient[i].g;
            stop["b"] = gradient[i].b;
        }

        JsonObject sch = doc.createNestedObject("schedule");
        sch["enabled"] = schedule.enabled;
        sch["on_hour"] = schedule.on_hour;
        sch["on_min"] = schedule.on_min;
        sch["off_hour"] = schedule.off_hour;
        sch["off_min"] = schedule.off_min;

        JsonObject tmr = doc.createNestedObject("timer");
        tmr["active"] = timer.active;
        if (timer.active) {
            uint32_t elapsed = (millis() - timer.start_ms) / 1000;
            if (elapsed < timer.duration_sec) {
                tmr["remaining_sec"] = timer.duration_sec - elapsed;
            } else {
                tmr["remaining_sec"] = 0;
            }
        } else {
            tmr["remaining_sec"] = 0;
        }
        tmr["gradual_dim"] = timer.gradual_dim;

        String output;
        serializeJson(doc, output);
        return output;
    }
};

extern ConfigManager config;
