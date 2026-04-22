#include "win_byte_fix.h"
#include "Preferences.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

// ── Helpers ─────────────────────────────────────────────────────────────────

std::string Preferences::getPrefsDir() const {
    const char* home = getenv("HOME");
#ifdef TARGET_WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (!home) home = "/tmp";
    return std::string(home) + "/.virtualstage";
}

std::string Preferences::getPrefsPath() const {
    return getPrefsDir() + "/preferences.json";
}

static std::string unitToString(MeasurementUnit u) {
    switch (u) {
        case MeasurementUnit::Meters:      return "meters";
        case MeasurementUnit::Centimeters: return "centimeters";
        case MeasurementUnit::Feet:        return "feet";
        case MeasurementUnit::Inches:      return "inches";
    }
    return "meters";
}

static MeasurementUnit stringToUnit(const std::string& s) {
    if (s == "centimeters") return MeasurementUnit::Centimeters;
    if (s == "feet")        return MeasurementUnit::Feet;
    if (s == "inches")      return MeasurementUnit::Inches;
    return MeasurementUnit::Meters;
}

static std::string bgModeToString(BackgroundMode m) {
    switch (m) {
        case BackgroundMode::Solid:    return "solid";
        case BackgroundMode::Gradient: return "gradient";
        case BackgroundMode::Image:    return "image";
    }
    return "solid";
}

static BackgroundMode stringToBgMode(const std::string& s) {
    if (s == "gradient") return BackgroundMode::Gradient;
    if (s == "image")    return BackgroundMode::Image;
    return BackgroundMode::Solid;
}

static ofJson colorToJson(const ofColor& c) {
    return {{"r", c.r}, {"g", c.g}, {"b", c.b}};
}

static ofColor jsonToColor(const ofJson& j, const ofColor& def) {
    if (!j.is_object()) return def;
    return ofColor(j.value("r", (int)def.r), j.value("g", (int)def.g), j.value("b", (int)def.b));
}

// ── Local I/O ───────────────────────────────────────────────────────────────

void Preferences::loadLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    std::ifstream f(getPrefsPath());
    if (!f.is_open()) return;
    try {
        ofJson j = ofJson::parse(f);
        if (j.contains("measurementUnit") && j["measurementUnit"].is_string()) {
            unit = stringToUnit(j["measurementUnit"].get<std::string>());
        }
        if (j.contains("bgMode") && j["bgMode"].is_string()) {
            bgMode = stringToBgMode(j["bgMode"].get<std::string>());
        }
        if (j.contains("bgColor")) bgColor = jsonToColor(j["bgColor"], bgColor);
        if (j.contains("bgGradientTop")) bgGradientTop = jsonToColor(j["bgGradientTop"], bgGradientTop);
        if (j.contains("bgGradientBottom")) bgGradientBottom = jsonToColor(j["bgGradientBottom"], bgGradientBottom);
        if (j.contains("bgImagePath") && j["bgImagePath"].is_string()) {
            bgImagePath = j["bgImagePath"].get<std::string>();
        }
        if (j.contains("hideStatusBarInView")) hideStatusBarInView = j.value("hideStatusBarInView", false);
        if (j.contains("hideTitleBarInView")) hideTitleBarInView = j.value("hideTitleBarInView", false);
    } catch (...) {}
}

void Preferences::saveLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    // Ensure directory exists
    std::string dir = getPrefsDir();
#ifdef TARGET_WIN32
    mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif

    ofJson j;
    j["measurementUnit"] = unitToString(unit);
    j["bgMode"] = bgModeToString(bgMode);
    j["bgColor"] = colorToJson(bgColor);
    j["bgGradientTop"] = colorToJson(bgGradientTop);
    j["bgGradientBottom"] = colorToJson(bgGradientBottom);
    if (!bgImagePath.empty()) j["bgImagePath"] = bgImagePath;
    j["hideStatusBarInView"] = hideStatusBarInView;
    j["hideTitleBarInView"] = hideTitleBarInView;

    std::ofstream f(getPrefsPath());
    if (f.is_open()) {
        f << j.dump(4);
    }
}

// ── Getters / setters ───────────────────────────────────────────────────────

MeasurementUnit Preferences::getUnit() const {
    std::lock_guard<std::mutex> lock(mtx);
    return unit;
}

void Preferences::setUnit(MeasurementUnit u) {
    std::lock_guard<std::mutex> lock(mtx);
    unit = u;
}

std::string Preferences::getUnitSuffix() const {
    std::lock_guard<std::mutex> lock(mtx);
    switch (unit) {
        case MeasurementUnit::Meters:      return "m";
        case MeasurementUnit::Centimeters: return "cm";
        case MeasurementUnit::Feet:        return "ft";
        case MeasurementUnit::Inches:      return "in";
    }
    return "m";
}

// ── Background getters / setters ────────────────────────────────────────────

BackgroundMode Preferences::getBgMode() const {
    std::lock_guard<std::mutex> lock(mtx);
    return bgMode;
}

void Preferences::setBgMode(BackgroundMode m) {
    std::lock_guard<std::mutex> lock(mtx);
    bgMode = m;
}

ofColor Preferences::getBgColor() const {
    std::lock_guard<std::mutex> lock(mtx);
    return bgColor;
}

void Preferences::setBgColor(const ofColor& c) {
    std::lock_guard<std::mutex> lock(mtx);
    bgColor = c;
}

ofColor Preferences::getBgGradientTop() const {
    std::lock_guard<std::mutex> lock(mtx);
    return bgGradientTop;
}

void Preferences::setBgGradientTop(const ofColor& c) {
    std::lock_guard<std::mutex> lock(mtx);
    bgGradientTop = c;
}

ofColor Preferences::getBgGradientBottom() const {
    std::lock_guard<std::mutex> lock(mtx);
    return bgGradientBottom;
}

void Preferences::setBgGradientBottom(const ofColor& c) {
    std::lock_guard<std::mutex> lock(mtx);
    bgGradientBottom = c;
}

std::string Preferences::getBgImagePath() const {
    std::lock_guard<std::mutex> lock(mtx);
    return bgImagePath;
}

void Preferences::setBgImagePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx);
    bgImagePath = path;
}

bool Preferences::getHideStatusBarInView() const {
    std::lock_guard<std::mutex> lock(mtx);
    return hideStatusBarInView;
}

void Preferences::setHideStatusBarInView(bool v) {
    std::lock_guard<std::mutex> lock(mtx);
    hideStatusBarInView = v;
}

bool Preferences::getHideTitleBarInView() const {
    std::lock_guard<std::mutex> lock(mtx);
    return hideTitleBarInView;
}

void Preferences::setHideTitleBarInView(bool v) {
    std::lock_guard<std::mutex> lock(mtx);
    hideTitleBarInView = v;
}

// ── Conversion ──────────────────────────────────────────────────────────────
// Base mapping: 1 OGL unit = 1 cm → 100 OGL = 1 m

float Preferences::oglPerDisplayUnit() const {
    std::lock_guard<std::mutex> lock(mtx);
    switch (unit) {
        case MeasurementUnit::Meters:      return 100.0f;
        case MeasurementUnit::Centimeters: return 1.0f;
        case MeasurementUnit::Feet:        return 30.48f;
        case MeasurementUnit::Inches:      return 2.54f;
    }
    return 100.0f;
}

float Preferences::oglToDisplay(float oglValue) const {
    return oglValue / oglPerDisplayUnit();
}

float Preferences::displayToOgl(float displayValue) const {
    return displayValue * oglPerDisplayUnit();
}

// ── Cloud sync serialization ────────────────────────────────────────────────

std::string Preferences::toJsonString() const {
    std::lock_guard<std::mutex> lock(mtx);
    ofJson j;
    j["measurementUnit"] = unitToString(unit);
    j["bgMode"] = bgModeToString(bgMode);
    j["bgColor"] = colorToJson(bgColor);
    j["bgGradientTop"] = colorToJson(bgGradientTop);
    j["bgGradientBottom"] = colorToJson(bgGradientBottom);
    if (!bgImagePath.empty()) j["bgImagePath"] = bgImagePath;
    j["hideStatusBarInView"] = hideStatusBarInView;
    j["hideTitleBarInView"] = hideTitleBarInView;
    return j.dump();
}

void Preferences::fromJsonString(const std::string& jsonStr) {
    std::lock_guard<std::mutex> lock(mtx);
    try {
        ofJson j = ofJson::parse(jsonStr);
        if (j.contains("measurementUnit") && j["measurementUnit"].is_string()) {
            unit = stringToUnit(j["measurementUnit"].get<std::string>());
        }
        if (j.contains("bgMode") && j["bgMode"].is_string()) {
            bgMode = stringToBgMode(j["bgMode"].get<std::string>());
        }
        if (j.contains("bgColor")) bgColor = jsonToColor(j["bgColor"], bgColor);
        if (j.contains("bgGradientTop")) bgGradientTop = jsonToColor(j["bgGradientTop"], bgGradientTop);
        if (j.contains("bgGradientBottom")) bgGradientBottom = jsonToColor(j["bgGradientBottom"], bgGradientBottom);
        if (j.contains("bgImagePath") && j["bgImagePath"].is_string()) {
            bgImagePath = j["bgImagePath"].get<std::string>();
        }
        if (j.contains("hideStatusBarInView")) hideStatusBarInView = j.value("hideStatusBarInView", false);
        if (j.contains("hideTitleBarInView")) hideTitleBarInView = j.value("hideTitleBarInView", false);
    } catch (...) {}
}
