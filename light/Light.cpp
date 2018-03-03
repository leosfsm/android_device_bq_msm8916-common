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

#include "Light.h"

#include <android-base/logging.h>

namespace {
using android::hardware::light::V2_0::LightState;

static uint32_t rgbToBrightness(const LightState& state) {
    uint32_t color = state.color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}

static bool isLit(const LightState& state) {
    return (state.color & 0x00ffffff);
}
}  // anonymous namespace

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

Light::Light(std::ofstream&& backlight,
             std::ofstream&& button_backlight,
             std::ofstream&& red_led, std::ofstream&& green_led, std::ofstream&& blue_led,
             std::ofstream&& red_blink, std::ofstream&& green_blink, std::ofstream&& blue_blink)
    : mBacklight(std::move(backlight)),
      mButtonBacklight(std::move(button_backlight)),
      mRedLed(std::move(red_led)),
      mGreenLed(std::move(green_led)),
      mBlueLed(std::move(blue_led)),
      mRedBlink(std::move(red_blink)),
      mGreenBlink(std::move(green_blink)),
      mBlueBlink(std::move(blue_blink)){
    auto attnFn(std::bind(&Light::setAttentionLight, this, std::placeholders::_1));
    auto backlightFn(std::bind(&Light::setBacklight, this, std::placeholders::_1));
    auto batteryFn(std::bind(&Light::setBatteryLight, this, std::placeholders::_1));
    auto buttonsFn(std::bind(&Light::setButtonsBacklight, this, std::placeholders::_1));
    auto notifFn(std::bind(&Light::setNotificationLight, this, std::placeholders::_1));
    mLights.emplace(std::make_pair(Type::ATTENTION, attnFn));
    mLights.emplace(std::make_pair(Type::BACKLIGHT, backlightFn));
    mLights.emplace(std::make_pair(Type::BATTERY, batteryFn));
    mLights.emplace(std::make_pair(Type::BUTTONS, buttonsFn));
    mLights.emplace(std::make_pair(Type::NOTIFICATIONS, notifFn));
}

// Methods from ::android::hardware::light::V2_0::ILight follow.
Return<Status> Light::setLight(Type type, const LightState& state) {
    auto it = mLights.find(type);

    if (it == mLights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    it->second(state);

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (auto const& light : mLights) {
        types.push_back(light.first);
    }

    _hidl_cb(types);

    return Void();
}

void Light::setAttentionLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mAttentionState = state;
    setSpeakerBatteryLightLocked();
}

void Light::setBacklight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    uint32_t brightness = rgbToBrightness(state);

    mBacklight << brightness << std::endl;
}

void Light::setButtonsBacklight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mButtonBacklight << (state.color & 0xff) << std::endl;
}

void Light::setBatteryLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mBatteryState = state;
    setSpeakerBatteryLightLocked();
}

void Light::setNotificationLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mNotificationState = state;
    setSpeakerBatteryLightLocked();
}


void Light::setSpeakerBatteryLightLocked() {
    if (isLit(mNotificationState)) {
        setSpeakerLightLocked(mNotificationState);
    } else if (isLit(mAttentionState)) {
        setSpeakerLightLocked(mAttentionState);
    } else if (isLit(mBatteryState)) {
        setSpeakerLightLocked(mBatteryState);
    } else {
        // Lights off
        mRedLed << 0 << std::endl;
        mGreenLed << 0 << std::endl;
        mBlueLed << 0 << std::endl;
        mRedBlink << 0 << std::endl;
        mGreenBlink << 0 << std::endl;
        mBlueBlink << 0 << std::endl;
    }
}

void Light::setSpeakerLightLocked(const LightState& state) {
    int red, green, blue, blink;
    int onMs, offMs;
    int color;
    char buffer[40];
    uint32_t colorRGB = state.color;

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

    snprintf(buffer, sizeof(buffer), "%d %d %d\n", color, onMs, offMs);

    if (blink) {
        if(red) {
        color = red;
        mRedBlink << buffer << std::endl;
      } else
        mRedLed << red << std::endl;
        if(green) {
        color = green;
        mGreenBlink << buffer << std::endl; 
      } else
        mGreenLed << green << std::endl;
        if(blue) {
        color = blue;
        mBlueBlink << buffer << std::endl;
    } else
        mBlueLed << blue << std::endl;
    } else {
        mRedLed << red << std::endl;
        mGreenLed << green << std::endl;
        mBlueLed << blue << std::endl;
    }
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
