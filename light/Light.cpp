/*
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LightService"

#include <log/log.h>

#include "Light.h"

#include <fstream>

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

#define LCD_LED         "/sys/class/leds/lcd-backlight/"
#define BUTTON_LED      "/sys/class/leds/button-backlight/"
#define RED_LED         "/sys/class/leds/red/"
#define GREEN_LED       "/sys/class/leds/green/"
#define BLUE_LED        "/sys/class/leds/blue/"

#define BRIGHTNESS      "brightness"
#define BLINK           "blink"

/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);

    if (!file.is_open()) {
        ALOGE("failed to write %s to %s", value.c_str(), path.c_str());
        return;
    }

    file << value;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static void set_blink(std::string path, int value, int on, int off) {
    std::ofstream file(path);
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%d %d %d\n", value, on, off);

    if (!file.is_open()) {
        ALOGE("failed to write %d to %s", value, path.c_str());
        return;
    }

    file << buffer;
}

static uint32_t rgbToBrightness(const LightState& state) {
    uint32_t color = state.color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}

static void handleBacklight(const LightState& state) {
    uint32_t brightness = rgbToBrightness(state);
    set(LCD_LED BRIGHTNESS, brightness);
}

static void handleButtons(const LightState& state) {
    uint32_t brightness = state.color & 0xFF;
    set(BUTTON_LED BRIGHTNESS, brightness);
}

static void handleNotification(const LightState& state) {
    uint32_t red, green, blue;
    uint32_t colorRGB = state.color;
    int blink, onMs, offMs;

    switch (state.flashMode) {
        case Flash::TIMED:
            onMs = state.flashOnMs;
            offMs = state.flashOffMs;
            break;
        case Flash::NONE:
        default:
            onMs = 0;
            offMs = 0;
            break;
    }

    red = (colorRGB >> 16) & 0xff;
    green = (colorRGB >> 8) & 0xff;
    blue = colorRGB & 0xff;
    if (onMs > 0 && offMs > 0) {
        blink = onMs;
    } else {
        blink = 0;
    }

    switch(blink)
    {
    case 0:
        set(RED_LED BRIGHTNESS, red);
        set(GREEN_LED BRIGHTNESS, green);
        set(BLUE_LED BRIGHTNESS, blue);
        break;
    default:
        if(red)
    set_blink(RED_LED BLINK, red, onMs, offMs);
        else
        set(RED_LED BRIGHTNESS, red);
        if(green)
    set_blink(GREEN_LED BLINK, green, onMs, offMs);
        else
        set(GREEN_LED BRIGHTNESS, green);
        if(blue)
    set_blink(BLUE_LED BLINK, blue, onMs, offMs);
        else
        set(BLUE_LED BRIGHTNESS, blue);
        break;
    }
}

static std::map<Type, std::function<void(const LightState&)>> lights = {
    {Type::BACKLIGHT, handleBacklight},
    {Type::BUTTONS, handleButtons},
    {Type::BATTERY, handleNotification},
    {Type::NOTIFICATIONS, handleNotification},
    {Type::ATTENTION, handleNotification},
};

Light::Light() {}

Return<Status> Light::setLight(Type type, const LightState& state) {
    auto it = lights.find(type);

    if (it == lights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    /*
     * Lock global mutex until light state is updated.
     */
    std::lock_guard<std::mutex> lock(globalLock);

    it->second(state);

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (auto const& light : lights) types.push_back(light.first);

    _hidl_cb(types);

    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
