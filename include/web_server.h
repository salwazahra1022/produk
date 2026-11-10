#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include "config_manager.h"
#include "led_effects.h"
#include "ntp_manager.h"

class WebServerManager {
private:
    AsyncWebServer server;
    AsyncWebSocket ws;

    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0; // null terminate

            StaticJsonDocument<1536> doc;
            DeserializationError error = deserializeJson(doc, (char*)data);
            if (error) {
                Serial.printf("[WebSocket] JSON parse error: %s\n", error.c_str());
                return;
            }

            const char* cmd = doc["cmd"] | "";

            if (strcmp(cmd, "power") == 0) {
                config.power = doc["state"] | !config.power;
                config.markDirty();
            } else if (strcmp(cmd, "color") == 0) {
                config.color.r = doc["r"] | config.color.r;
                config.color.g = doc["g"] | config.color.g;
                config.color.b = doc["b"] | config.color.b;
                // If in non-color effect or temp mode, switch back to Solid
                if (config.effect_mode == 21 || config.effect_mode == 22) {
                    config.effect_mode = 0;
                }
                config.markDirty();
            } else if (strcmp(cmd, "brightness") == 0) {
                config.brightness = doc["val"] | config.brightness;
                config.markDirty();
            } else if (strcmp(cmd, "temp") == 0) {
                config.color_temp = doc["val"] | config.color_temp;
                config.effect_mode = 22; // Color Temperature mode
                config.markDirty();
            } else if (strcmp(cmd, "effect") == 0) {
                config.effect_mode = doc["id"] | 0;
                if (doc.containsKey("speed")) config.speed = doc["speed"];
                if (doc.containsKey("intensity")) config.intensity = doc["intensity"];
                config.markDirty();
            } else if (strcmp(cmd, "speed") == 0) {
                config.speed = doc["val"] | config.speed;
                config.markDirty();
            } else if (strcmp(cmd, "intensity") == 0) {
                config.intensity = doc["val"] | config.intensity;
                config.markDirty();
            } else if (strcmp(cmd, "gradient") == 0) {
                if (doc.containsKey("stops") && doc["stops"].is<JsonArray>()) {
                    JsonArray stops = doc["stops"];
                    config.gradient_count = stops.size();
                    if (config.gradient_count > 4) config.gradient_count = 4;
                    if (config.gradient_count < 2) config.gradient_count = 2;

                    for (size_t i = 0; i < config.gradient_count; i++) {
                        config.gradient[i].r = stops[i]["r"] | 0;
                        config.gradient[i].g = stops[i]["g"] | 0;
                        config.gradient[i].b = stops[i]["b"] | 0;
                    }
                    config.effect_mode = 21; // Switch to Custom Gradient mode
                    config.markDirty();
                }
            } else if (strcmp(cmd, "timer") == 0) {
                bool enable = doc["enable"] | false;
                if (enable) {
                    uint32_t mins = doc["minutes"] | 15;
                    config.timer.active = true;
                    config.timer.duration_sec = mins * 60;
                    config.timer.start_ms = millis();
                    config.timer.gradual_dim = doc["gradual"] | false;
                    config.timer.initial_brightness = config.brightness;
                } else {
                    config.timer.active = false;
                }
            } else if (strcmp(cmd, "schedule") == 0) {
                config.schedule.enabled = doc["enabled"] | false;
                const char* on_str = doc["on"] | "18:00";
                const char* off_str = doc["off"] | "06:00";
                
                int on_h = 18, on_m = 0, off_h = 6, off_m = 0;
                sscanf(on_str, "%d:%d", &on_h, &on_m);
                sscanf(off_str, "%d:%d", &off_h, &off_m);

                config.schedule.on_hour = (uint8_t)constrain(on_h, 0, 23);
                config.schedule.on_min = (uint8_t)constrain(on_m, 0, 59);
                config.schedule.off_hour = (uint8_t)constrain(off_h, 0, 23);
                config.schedule.off_min = (uint8_t)constrain(off_m, 0, 59);
                config.markDirty();
            } else if (strcmp(cmd, "settings") == 0) {
                if (doc.containsKey("num_leds")) {
                    config.num_leds = doc["num_leds"];
                    if (config.num_leds > MAX_LEDS_LIMIT) config.num_leds = MAX_LEDS_LIMIT;
                    if (config.num_leds < 1) config.num_leds = 1;
                }
                if (doc.containsKey("max_ma")) {
                    config.max_ma = doc["max_ma"];
                    if (config.max_ma < 200) config.max_ma = 200;
                    ledController.updatePowerLimit();
                }
                config.markDirty();
            } else if (strcmp(cmd, "reset_wifi") == 0) {
                Serial.println(F("[WiFi] User requested WiFi reset via Web UI."));
                WiFi.disconnect(true);
                ESP.eraseConfig();
                delay(500);
                ESP.restart();
            } else if (strcmp(cmd, "get_status") == 0) {
                sendStatus(client);
                return;
            }

            // Broadcast new state to all connected clients
            broadcastState();
        }
    }

    void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("[WebSocket] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
                client->text(config.getStateJson());
                sendStatus(client);
                break;
            case WS_EVT_DISCONNECT:
                Serial.printf("[WebSocket] Client #%u disconnected\n", client->id());
                break;
            case WS_EVT_DATA:
                handleWebSocketMessage(arg, data, len, client);
                break;
            case WS_EVT_PONG:
            case WS_EVT_ERROR:
                break;
        }
    }

public:
    WebServerManager() : server(80), ws("/ws") {}

    void begin() {
        // Setup WebSocket
        ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            this->onEvent(server, client, type, arg, data, len);
        });
        server.addHandler(&ws);

        // Serve Static LittleFS files
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (LittleFS.exists("/index.html")) {
                request->send(LittleFS, "/index.html", "text/html");
            } else {
                request->send(200, "text/plain", "Smart LED Controller: index.html not found in LittleFS. Please upload filesystem image.");
            }
        });

        server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (LittleFS.exists("/style.css")) {
                AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/style.css", "text/css");
                response->addHeader("Cache-Control", "public, max-age=86400");
                request->send(response);
            } else {
                request->send(404, "text/plain", "style.css not found");
            }
        });

        server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (LittleFS.exists("/app.js")) {
                AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/app.js", "application/javascript");
                response->addHeader("Cache-Control", "public, max-age=86400");
                request->send(response);
            } else {
                request->send(404, "text/plain", "app.js not found");
            }
        });

        // Diagnostic API endpoint
        server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
            StaticJsonDocument<512> doc;
            doc["free_heap"] = ESP.getFreeHeap();
            doc["heap_frag"] = ESP.getHeapFragmentation();
            doc["rssi"] = WiFi.RSSI();
            doc["ip"] = WiFi.localIP().toString();
            doc["uptime"] = millis() / 1000;
            doc["time"] = ntpManager.getFormattedTime();
            doc["ntp_synced"] = ntpManager.isSynced();
            doc["clients"] = ws.count();
            String res;
            serializeJson(doc, res);
            request->send(200, "application/json", res);
        });

        // Initialize ElegantOTA
        ElegantOTA.begin(&server);

        // Fallback static files
        server.serveStatic("/", LittleFS, "/");

        server.begin();
        Serial.println(F("[WebServer] AsyncWebServer started on port 80 with ElegantOTA at /update"));
    }

    void cleanupClients() {
        ws.cleanupClients();
    }

    void broadcastState() {
        if (ws.count() > 0) {
            String state = config.getStateJson();
            ws.textAll(state);
        }
    }

    void sendStatus(AsyncWebSocketClient *client) {
        StaticJsonDocument<512> doc;
        doc["type"] = "diag";
        doc["free_heap"] = ESP.getFreeHeap();
        doc["rssi"] = WiFi.RSSI();
        doc["ip"] = WiFi.localIP().toString();
        doc["uptime"] = millis() / 1000;
        doc["time"] = ntpManager.getFormattedTime();
        doc["ntp_synced"] = ntpManager.isSynced();
        doc["clients"] = ws.count();
        String res;
        serializeJson(doc, res);
        if (client) {
            client->text(res);
        } else {
            ws.textAll(res);
        }
    }
};

extern WebServerManager webServerManager;
