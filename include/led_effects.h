#pragma once
#include <Arduino.h>
#define FASTLED_INTERNAL
#include <FastLED.h>
#include "config_manager.h"
#include "ntp_manager.h"

#define LED_PIN 2 // D4 on NodeMCU / Wemos D1 Mini is GPIO2

class LEDController {
private:
    CRGB leds[MAX_LEDS_LIMIT];
    uint8_t heat[MAX_LEDS_LIMIT];
    uint32_t last_frame_ms = 0;
    uint32_t last_ntp_check_ms = 0;
    uint8_t step_counter = 0;
    int scanner_pos = 0;
    int scanner_dir = 1;
    bool state_changed_by_timer = false;

    CRGB kelvinToRGB(uint16_t kelvin) {
        float temp = kelvin / 100.0f;
        float r, g, b;

        // Red
        if (temp <= 66.0f) {
            r = 255.0f;
        } else {
            r = temp - 60.0f;
            r = 329.698727446f * pow(r, -0.1332047592f);
            if (r < 0) r = 0;
            if (r > 255) r = 255;
        }

        // Green
        if (temp <= 66.0f) {
            g = temp;
            g = 99.4708025861f * log(g) - 161.1195681661f;
            if (g < 0) g = 0;
            if (g > 255) g = 255;
        } else {
            g = temp - 60.0f;
            g = 288.1221695283f * pow(g, -0.0755148492f);
            if (g < 0) g = 0;
            if (g > 255) g = 255;
        }

        // Blue
        if (temp >= 66.0f) {
            b = 255.0f;
        } else if (temp <= 19.0f) {
            b = 0.0f;
        } else {
            b = temp - 10.0f;
            b = 138.5177312231f * log(b) - 305.0447927307f;
            if (b < 0) b = 0;
            if (b > 255) b = 255;
        }

        return CRGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }

public:
    void begin() {
        FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS_LIMIT).setCorrection(TypicalLEDStrip);
        FastLED.setMaxPowerInVoltsAndMilliamps(5, config.max_ma);
        FastLED.setBrightness(config.brightness);
        fill_solid(leds, MAX_LEDS_LIMIT, CRGB::Black);
        FastLED.show();
        memset(heat, 0, sizeof(heat));
        Serial.printf("[FastLED] Initialized on GPIO%d with max %d LEDs, current limit %dmA\n", LED_PIN, MAX_LEDS_LIMIT, config.max_ma);
    }

    void updatePowerLimit() {
        FastLED.setMaxPowerInVoltsAndMilliamps(5, config.max_ma);
    }

    bool checkStateChanged() {
        if (state_changed_by_timer) {
            state_changed_by_timer = false;
            return true;
        }
        return false;
    }

    void update() {
        uint32_t now = millis();

        // Check Timer
        if (config.timer.active) {
            uint32_t elapsed_sec = (now - config.timer.start_ms) / 1000;
            if (elapsed_sec >= config.timer.duration_sec) {
                config.power = false;
                config.timer.active = false;
                config.brightness = config.timer.initial_brightness;
                state_changed_by_timer = true;
                config.markDirty();
            } else if (config.timer.gradual_dim) {
                // Dim down in the remaining portion of time (up to 300 seconds)
                uint32_t remaining = config.timer.duration_sec - elapsed_sec;
                uint32_t dim_window = min((uint32_t)300, config.timer.duration_sec);
                if (remaining <= dim_window && dim_window > 0) {
                    float factor = (float)remaining / (float)dim_window;
                    uint8_t dimmed = (uint8_t)(config.timer.initial_brightness * factor);
                    FastLED.setBrightness(dimmed);
                }
            }
        }

        // Check Daily Schedule (once per second)
        if (now - last_ntp_check_ms >= 1000) {
            last_ntp_check_ms = now;
            if (config.schedule.enabled && ntpManager.isSynced()) {
                int ch = ntpManager.getHours();
                int cm = ntpManager.getMinutes();

                if (ch == config.schedule.on_hour && cm == config.schedule.on_min) {
                    if (config.schedule.last_triggered_min != cm) {
                        config.power = true;
                        config.schedule.last_triggered_min = cm;
                        state_changed_by_timer = true;
                    }
                } else if (ch == config.schedule.off_hour && cm == config.schedule.off_min) {
                    if (config.schedule.last_triggered_min != cm) {
                        config.power = false;
                        config.schedule.last_triggered_min = cm;
                        state_changed_by_timer = true;
                    }
                }
            }
        }

        // Power OFF handler
        if (!config.power) {
            fill_solid(leds, MAX_LEDS_LIMIT, CRGB::Black);
            FastLED.show();
            return;
        }

        // Apply normal brightness if not dynamically overriding via timer
        if (!config.timer.active || !config.timer.gradual_dim) {
            FastLED.setBrightness(config.brightness);
        }

        // Calculate dynamic frame delay based on speed (1 - 100)
        // Speed 100 -> ~10ms (100 fps), Speed 1 -> ~100ms (10 fps)
        uint16_t frame_interval = map(config.speed, 1, 100, 100, 10);
        if (now - last_frame_ms < frame_interval) {
            return;
        }
        last_frame_ms = now;

        // Clear unused LEDs beyond configured count
        for (int i = config.num_leds; i < MAX_LEDS_LIMIT; i++) {
            leds[i] = CRGB::Black;
        }

        // Render Active Effect
        renderEffect();

        FastLED.show();
    }

    void renderEffect() {
        uint16_t count = config.num_leds;
        CRGB primary(config.color.r, config.color.g, config.color.b);

        switch (config.effect_mode) {
            case 0: // Solid Color
                fill_solid(leds, count, primary);
                break;

            case 1: { // Rainbow (Unified cycle)
                step_counter += 2;
                fill_solid(leds, count, CHSV(step_counter, 255, 255));
                break;
            }

            case 2: { // Rainbow Wave (Spatial moving wave)
                step_counter += map(config.speed, 1, 100, 1, 6);
                uint8_t delta_hue = map(config.intensity, 1, 100, 2, 20);
                fill_rainbow(leds, count, step_counter, delta_hue);
                break;
            }

            case 3: { // Breathing / Pulse
                float breath = (exp(sin(millis() / (1200.0f - (config.speed * 8.0f)) * PI)) - 0.36787944f) * 108.0f;
                uint8_t val = constrain((int)breath, 5, 255);
                CRGB c = primary;
                c.nscale8_video(val);
                fill_solid(leds, count, c);
                break;
            }

            case 4: { // Comet / Meteor Rain
                fadeToBlackBy(leds, count, map(config.intensity, 1, 100, 80, 20));
                step_counter = (step_counter + 1) % count;
                leds[step_counter] = primary;
                break;
            }

            case 5: { // Fire 2012
                int cooling = map(config.intensity, 1, 100, 40, 80);
                int sparking = map(config.speed, 1, 100, 80, 180);

                // Step 1. Cool down every cell a little
                for (int i = 0; i < count; i++) {
                    heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / count) + 2));
                }

                // Step 2. Heat from previous cells drift up
                for (int k = count - 1; k >= 2; k--) {
                    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
                }

                // Step 3. Randomly ignite new 'sparks' of heat near the bottom
                if (random8() < sparking) {
                    int y = random8(min((int)count, 7));
                    heat[y] = qadd8(heat[y], random8(160, 255));
                }

                // Step 4. Map heat cells to LED colors
                for (int j = 0; j < count; j++) {
                    leds[j] = HeatColor(heat[j]);
                }
                break;
            }

            case 6: { // Twinkle / Sparkle
                fadeToBlackBy(leds, count, 35);
                uint8_t num_sparks = map(config.intensity, 1, 100, 1, 5);
                for (uint8_t s = 0; s < num_sparks; s++) {
                    if (random8() < 120) {
                        int pos = random16(count);
                        leds[pos] = primary;
                    }
                }
                break;
            }

            case 7: { // Police Strobe
                step_counter++;
                uint8_t strobe_state = (step_counter / 3) % 8;
                fill_solid(leds, count, CRGB::Black);
                int half = count / 2;
                if (strobe_state == 0 || strobe_state == 2) {
                    for (int i = 0; i < half; i++) leds[i] = CRGB::Red;
                } else if (strobe_state == 4 || strobe_state == 6) {
                    for (int i = half; i < count; i++) leds[i] = CRGB::Blue;
                }
                break;
            }

            case 8: { // Color Wipe
                step_counter = (step_counter + 1) % (count * 2);
                if (step_counter < count) {
                    leds[step_counter] = primary;
                } else {
                    leds[step_counter - count] = CRGB::Black;
                }
                break;
            }

            case 9: { // Theater Chase
                step_counter++;
                int spacing = 3;
                for (int i = 0; i < count; i++) {
                    if ((i + step_counter) % spacing == 0) {
                        leds[i] = primary;
                    } else {
                        leds[i] = CRGB::Black;
                    }
                }
                break;
            }

            case 10: { // Gradient Flow
                step_counter += 2;
                for (int i = 0; i < count; i++) {
                    uint8_t hue = step_counter + (i * 256 / count);
                    leds[i] = CHSV(hue, 220, 255);
                }
                break;
            }

            case 11: { // Pacman Chaser
                step_counter = (step_counter + 1) % count;
                fill_solid(leds, count, CRGB::Black);
                // Dots ahead of pacman
                for (int i = 0; i < count; i += 4) {
                    if (i > step_counter) leds[i] = CRGB(80, 80, 80);
                }
                // Ghost
                int ghost_pos = (step_counter + count - 5) % count;
                leds[ghost_pos] = CRGB::Red;
                // Pacman
                leds[step_counter] = CRGB::Yellow;
                break;
            }

            case 12: { // Sinelon (Smooth sine wave sweep with fading trail)
                fadeToBlackBy(leds, count, map(config.intensity, 1, 100, 10, 50));
                int pos = beatsin16(config.speed, 0, count - 1);
                leds[pos] |= primary;
                break;
            }

            case 13: { // Juggle (Eight colored dots bouncing)
                fadeToBlackBy(leds, count, 20);
                byte dothue = 0;
                for (int i = 0; i < 6; i++) {
                    int pos = beatsin16(i + 7, 0, count - 1);
                    leds[pos] |= CHSV(dothue, 200, 255);
                    dothue += 32;
                }
                break;
            }

            case 14: { // Matrix Rain (Cyberpunk digital drop)
                fadeToBlackBy(leds, count, 40);
                if (random8() < map(config.intensity, 1, 100, 30, 160)) {
                    int head = random16(count);
                    leds[head] = CRGB(180, 255, 220); // Bright white-green head
                }
                // Bleed green downward
                for (int i = count - 1; i > 0; i--) {
                    if (leds[i - 1].g > 50) {
                        leds[i] = CRGB(0, leds[i - 1].g - 30, 20);
                    }
                }
                break;
            }

            case 15: { // Strobe Flash
                step_counter++;
                if (step_counter % 6 == 0) {
                    fill_solid(leds, count, primary);
                } else {
                    fill_solid(leds, count, CRGB::Black);
                }
                break;
            }

            case 16: { // BPM Pulse
                uint8_t beat = beatsin8(map(config.speed, 1, 100, 40, 140), 64, 255);
                for (int i = 0; i < count; i++) {
                    leds[i] = ColorFromPalette(PartyColors_p, step_counter + (i * 2), beat - step_counter + (i * 10));
                }
                step_counter++;
                break;
            }

            case 17: { // Larson Scanner (Cylon Eye)
                fadeToBlackBy(leds, count, map(config.intensity, 1, 100, 30, 80));
                scanner_pos += scanner_dir;
                if (scanner_pos >= count - 1) {
                    scanner_pos = count - 1;
                    scanner_dir = -1;
                } else if (scanner_pos <= 0) {
                    scanner_pos = 0;
                    scanner_dir = 1;
                }
                leds[scanner_pos] = primary;
                if (scanner_pos > 0) leds[scanner_pos - 1] |= primary.nscale8(120);
                if (scanner_pos < count - 1) leds[scanner_pos + 1] |= primary.nscale8(120);
                break;
            }

            case 18: { // Ripple (Center outwards)
                fadeToBlackBy(leds, count, 25);
                step_counter++;
                int center = count / 2;
                int r_step = (step_counter / 2) % (center + 1);
                if (center - r_step >= 0) leds[center - r_step] |= primary;
                if (center + r_step < count) leds[center + r_step] |= primary;
                break;
            }

            case 19: { // Heartbeat
                float beat = beatsin8(map(config.speed, 1, 100, 40, 100), 0, 255);
                // Double thump curve
                uint8_t b_val = (beat > 180 || (beat > 70 && beat < 110)) ? 255 : 20;
                CRGB c = primary;
                c.nscale8_video(b_val);
                fill_solid(leds, count, c);
                break;
            }

            case 20: { // Confetti
                fadeToBlackBy(leds, count, 20);
                int pos = random16(count);
                leds[pos] += CHSV(step_counter + random8(64), 200, 255);
                step_counter += 2;
                break;
            }

            case 21: { // Custom Gradient Mode
                uint8_t stops = config.gradient_count;
                if (stops < 2) stops = 2;
                for (int i = 0; i < count; i++) {
                    float pos = (float)i / (float)(count - 1); // 0.0 to 1.0
                    float segment = pos * (stops - 1);
                    int idx = (int)segment;
                    if (idx >= stops - 1) {
                        idx = stops - 2;
                    }
                    float frac = segment - idx; // 0.0 to 1.0 between idx and idx+1

                    uint8_t r = config.gradient[idx].r + frac * ((float)config.gradient[idx + 1].r - config.gradient[idx].r);
                    uint8_t g = config.gradient[idx].g + frac * ((float)config.gradient[idx + 1].g - config.gradient[idx].g);
                    uint8_t b = config.gradient[idx].b + frac * ((float)config.gradient[idx + 1].b - config.gradient[idx].b);
                    leds[i] = CRGB(r, g, b);
                }
                break;
            }

            case 22: { // Color Temperature Mode
                CRGB tempColor = kelvinToRGB(config.color_temp);
                fill_solid(leds, count, tempColor);
                break;
            }

            default:
                fill_solid(leds, count, primary);
                break;
        }
    }
};

extern LEDController ledController;
